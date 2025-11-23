/**
 * @file github_client.cpp
 * @brief Implementation of GitHub API client and HTTP utilities.
 *
 * Contains the implementation of HTTP client interfaces, CURL-based HTTP
 * operations, and the main GitHubClient logic for autogithubpullmerge.
 */

#include "github_client.hpp"
#include "curl/curl.h"
#include "log.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace agpm {

namespace {

std::string to_lower_copy(const std::string &value);

std::shared_ptr<spdlog::logger> github_client_log() {
  static auto logger = [] {
    ensure_default_logger();
    return category_logger("github.client");
  }();
  return logger;
}

PullRequestCheckState interpret_check_state(const nlohmann::json &meta) {
  std::string checks_state;
  if (meta.contains("checks_state") && meta["checks_state"].is_string()) {
    checks_state = meta["checks_state"].get<std::string>();
  }
  if (checks_state.empty() && meta.contains("mergeable_state") &&
      meta["mergeable_state"].is_string()) {
    checks_state = meta["mergeable_state"].get<std::string>();
  }
  std::string normalized = to_lower_copy(checks_state);
  if (normalized == "clean" || normalized == "success" ||
      normalized == "passed" || normalized == "pass" ||
      normalized == "passing") {
    return PullRequestCheckState::Passed;
  }
  if (normalized == "failure" || normalized == "failed" ||
      normalized == "rejected" || normalized == "blocked" ||
      normalized == "unstable") {
    return PullRequestCheckState::Rejected;
  }
  return PullRequestCheckState::Unknown;
}

/**
 * Convert a shell-style glob pattern to a regular expression.
 *
 * @param glob Glob expression containing '*' and '?' wildcards.
 * @return std::regex equivalent capturing the same matching semantics.
 */
std::regex glob_to_regex(const std::string &glob) {
  std::string rx = "^";
  for (char c : glob) {
    switch (c) {
    case '*':
      rx += ".*";
      break;
    case '?':
      rx += '.';
      break;
    case '.':
    case '+':
    case '(':
    case ')':
    case '{':
    case '}':
    case '^':
    case '$':
    case '|':
    case '\\':
    case '[':
    case ']':
      rx += '\\';
      rx += c;
      break;
    default:
      rx += c;
    }
  }
  rx += '$';
  return std::regex(rx);
}

/**
 * Convert a mixed wildcard pattern to a regex string.
 *
 * @param value Pattern potentially containing '*' or '?' characters.
 * @return String representation of the regex body.
 */
std::string mixed_to_regex(const std::string &value) {
  std::string out;
  out.reserve(value.size() * 2);
  for (char c : value) {
    switch (c) {
    case '*':
      out += ".*";
      break;
    case '?':
      out += '.';
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

/**
 * Produce a lowercase copy of the given string using ASCII rules.
 *
 * @param value Input text.
 * @return Lowercase copy of the input.
 */
std::string to_lower_copy(const std::string &value) {
  std::string out = value;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

/**
 * Create a human readable error message for a CURL request.
 *
 * @param verb HTTP verb attempted.
 * @param url Request URL.
 * @param code CURL error code.
 * @param errbuf Optional buffer with extended error text.
 * @return Combined error description.
 */
std::string format_curl_error(const char *verb, const std::string &url,
                              CURLcode code, const char *errbuf) {
  std::ostringstream oss;
  oss << "curl " << verb;
  if (!url.empty()) {
    oss << ' ' << url;
  }
  oss << " failed: " << curl_easy_strerror(code);
  if (errbuf != nullptr && errbuf[0] != '\0') {
    oss << " - " << errbuf;
  }
  if (code == CURLE_OPERATION_TIMEDOUT) {
    oss << " - " << curl_easy_strerror(CURLE_COULDNT_CONNECT);
  }
  return oss.str();
}

/**
 * Check if a name matches any pattern in a collection.
 *
 * Patterns support prefixes such as "glob:" or "regex:" in addition to raw
 * glob and regex entries.
 *
 * @param name Candidate string to evaluate.
 * @param patterns List of pattern expressions.
 * @return True when a pattern matches the provided name.
 */
bool matches_pattern(const std::string &name,
                     const std::vector<std::string> &patterns) {
  return std::any_of(
      patterns.begin(), patterns.end(), [&name](const std::string &pattern) {
        const std::string raw = pattern;
        auto colon = raw.find(':');
        if (colon != std::string::npos) {
          std::string tag = to_lower_copy(raw.substr(0, colon));
          std::string value = raw.substr(colon + 1);
          if (tag == "prefix") {
            if (value.empty()) {
              return true;
            }
            return name.rfind(value, 0) == 0;
          }
          if (tag == "suffix") {
            if (value.empty()) {
              return true;
            }
            return name.size() >= value.size() &&
                   name.compare(name.size() - value.size(), value.size(),
                                value) == 0;
          }
          if (tag == "contains") {
            return name.find(value) != std::string::npos;
          }
          if (tag == "literal") {
            return name == value;
          }
          if (tag == "glob" || tag == "wildcard") {
            try {
              return std::regex_match(name, glob_to_regex(value));
            } catch (const std::regex_error &) {
              return false;
            }
          }
          if (tag == "regex") {
            try {
              return std::regex_match(name, std::regex(value));
            } catch (const std::regex_error &) {
              return false;
            }
          }
          if (tag == "mixed") {
            try {
              std::regex re("^" + mixed_to_regex(value) + "$");
              return std::regex_match(name, re);
            } catch (const std::regex_error &) {
              return false;
            }
          }
          // Unknown tag: fall through to default handling using the raw
          // pattern.
        }
        if (raw.find_first_of("*?") != std::string::npos) {
          try {
            return std::regex_match(name, glob_to_regex(raw));
          } catch (const std::regex_error &) {
            return false;
          }
        }
        return name == raw;
      });
}

/**
 * Determine whether a branch name refers to a default base branch.
 *
 * @param name Branch name to inspect.
 * @return True when the name is "main" or "master" (case insensitive).
 */
bool is_base_branch_name(const std::string &name) {
  const std::string lower = to_lower_copy(name);
  return lower == "main" || lower == "master";
}

/**
 * Determine whether a branch should be treated as protected.
 *
 * @param name Branch name to evaluate.
 * @param protected_patterns Inclusion patterns that mark branches as
 *        protected.
 * @param exclude_patterns Patterns that explicitly lift protection.
 * @return True if the branch is protected after considering excludes.
 */
bool is_protected_branch(const std::string &name,
                         const std::vector<std::string> &protected_patterns,
                         const std::vector<std::string> &exclude_patterns) {
  if (!protected_patterns.empty() &&
      matches_pattern(name, protected_patterns)) {
    if (!exclude_patterns.empty() && matches_pattern(name, exclude_patterns)) {
      return false;
    }
    return true;
  }
  return false;
}

std::string encode_ref_segment(const std::string &segment) {
  if (segment.empty()) {
    return segment;
  }
  static CurlHandle curl;
  char *escaped = curl_easy_escape(curl.get(), segment.c_str(),
                                   static_cast<int>(segment.size()));
  if (escaped == nullptr) {
    github_client_log()->warn(
        "Failed to percent-encode ref segment {}; using raw value", segment);
    return segment;
  }
  std::string encoded(escaped);
  curl_free(escaped);
  return encoded;
}

} // namespace

/**
 * RAII wrapper managing a CURL linked list of headers.
 */
struct CurlSlist {
  curl_slist *list{nullptr};
  CurlSlist() = default;
  ~CurlSlist() { curl_slist_free_all(list); }
  void append(const std::string &s) {
    list = curl_slist_append(list, s.c_str());
  }
  curl_slist *get() const { return list; }
  CurlSlist(const CurlSlist &) = delete;
  CurlSlist &operator=(const CurlSlist &) = delete;
};

/**
 * Initialize the CURL handle, ensuring global setup occurs once.
 */
CurlHandle::CurlHandle() {
  static std::once_flag flag;
  std::call_once(flag, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
  handle_ = curl_easy_init();
  if (!handle_) {
    throw TransientNetworkError("Failed to init curl");
  }
}

/**
 * Clean up the CURL easy handle.
 */
CurlHandle::~CurlHandle() { curl_easy_cleanup(handle_); }

CurlHttpClient::CurlHttpClient(long timeout_ms, curl_off_t download_limit,
                               curl_off_t upload_limit, curl_off_t max_download,
                               curl_off_t max_upload, std::string http_proxy,
                               std::string https_proxy)
    : timeout_ms_(timeout_ms), download_limit_(download_limit),
      upload_limit_(upload_limit), max_download_(max_download),
      max_upload_(max_upload), http_proxy_(std::move(http_proxy)),
      https_proxy_(std::move(https_proxy)) {}

/**
 * libcurl write callback capturing response bodies into a string.
 */
static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t total = size * nmemb;
  std::string *s = static_cast<std::string *>(userp);
  s->append(static_cast<char *>(contents), total);
  return total;
}

/**
 * libcurl header callback collecting response headers.
 */
static size_t header_callback(char *buffer, size_t size, size_t nitems,
                              void *userdata) {
  size_t total = size * nitems;
  std::string line(buffer, total);
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
    line.pop_back();
  auto *hdrs = static_cast<std::vector<std::string> *>(userdata);
  hdrs->push_back(line);
  return total;
}

/**
 * Configure proxy settings on the CURL handle based on the request URL.
 *
 * @param curl CURL handle being prepared.
 * @param url Request target URL.
 */
void CurlHttpClient::apply_proxy(CURL *curl, const std::string &url) {
  const std::string *proxy = nullptr;
  if (url.rfind("https://", 0) == 0) {
    if (!https_proxy_.empty()) {
      proxy = &https_proxy_;
    } else if (!http_proxy_.empty()) {
      proxy = &http_proxy_;
    }
    if (proxy) {
      curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L);
    }
  } else if (url.rfind("http://", 0) == 0) {
    if (!http_proxy_.empty()) {
      proxy = &http_proxy_;
    }
    if (proxy) {
      curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 0L);
    }
  }
  if (proxy) {
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy->c_str());
  }
}

/**
 * Perform a GET request capturing both body and headers.
 */
HttpResponse
CurlHttpClient::get_with_headers(const std::string &url,
                                 const std::vector<std::string> &headers) {
  CURL *curl = curl_.get();
  curl_easy_reset(curl);
  std::string response;
  std::vector<std::string> resp_headers;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  apply_proxy(curl, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp_headers);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
  if (download_limit_ > 0)
    curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, download_limit_);
  if (upload_limit_ > 0)
    curl_easy_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, upload_limit_);
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = '\0';
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  CurlSlist header_list;
  for (const auto &h : headers) {
    header_list.append(h);
  }
  header_list.append("User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list.get());
  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_off_t dl = 0;
  curl_off_t ul = 0;
  curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &dl);
  curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD_T, &ul);
  total_downloaded_ += dl;
  total_uploaded_ += ul;
  if (max_download_ > 0 && total_downloaded_ > max_download_) {
    github_client_log()->error("Maximum download exceeded");
    throw std::runtime_error("Maximum download exceeded");
  }
  if (max_upload_ > 0 && total_uploaded_ > max_upload_) {
    github_client_log()->error("Maximum upload exceeded");
    throw std::runtime_error("Maximum upload exceeded");
  }
  if (res != CURLE_OK) {
    std::string msg = format_curl_error("GET", url, res, errbuf);
    github_client_log()->error(msg);
    throw TransientNetworkError(msg);
  }
  if (http_code < 200 || http_code >= 300) {
    if (http_code == 403 || http_code == 429) {
      // Let caller handle rate limiting
      return {response, resp_headers, http_code};
    }
    github_client_log()->error("curl GET {} failed with HTTP code {}", url,
                               http_code);
    throw HttpStatusError(static_cast<int>(http_code),
                          "curl GET failed with HTTP code " +
                              std::to_string(http_code));
  }
  return {response, resp_headers, http_code};
}

/**
 * Issue a GET request returning only the response body.
 */
std::string CurlHttpClient::get(const std::string &url,
                                const std::vector<std::string> &headers) {
  return get_with_headers(url, headers).body;
}

/**
 * Issue a PUT request with the provided payload.
 */
std::string CurlHttpClient::put(const std::string &url, const std::string &data,
                                const std::vector<std::string> &headers) {
  CURL *curl = curl_.get();
  curl_easy_reset(curl);
  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  apply_proxy(curl, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
  if (download_limit_ > 0)
    curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, download_limit_);
  if (upload_limit_ > 0)
    curl_easy_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, upload_limit_);
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = '\0';
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  CurlSlist header_list;
  for (const auto &h : headers) {
    header_list.append(h);
  }
  header_list.append("User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list.get());
  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_off_t dl = 0;
  curl_off_t ul = 0;
  curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &dl);
  curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD_T, &ul);
  total_downloaded_ += dl;
  total_uploaded_ += ul;
  if (max_download_ > 0 && total_downloaded_ > max_download_) {
    github_client_log()->error("Maximum download exceeded");
    throw std::runtime_error("Maximum download exceeded");
  }
  if (max_upload_ > 0 && total_uploaded_ > max_upload_) {
    github_client_log()->error("Maximum upload exceeded");
    throw std::runtime_error("Maximum upload exceeded");
  }
  if (res != CURLE_OK) {
    std::string msg = format_curl_error("PUT", url, res, errbuf);
    github_client_log()->error(msg);
    throw TransientNetworkError(msg);
  }
  if (http_code < 200 || http_code >= 300) {
    github_client_log()->error("curl PUT {} failed with HTTP code {}", url,
                               http_code);
    throw HttpStatusError(static_cast<int>(http_code),
                          "curl PUT failed with HTTP code " +
                              std::to_string(http_code));
  }
  return response;
}

/**
 * Issue a PATCH request with the provided payload.
 */
std::string CurlHttpClient::patch(const std::string &url,
                                  const std::string &data,
                                  const std::vector<std::string> &headers) {
  CURL *curl = curl_.get();
  curl_easy_reset(curl);
  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  apply_proxy(curl, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
  if (download_limit_ > 0)
    curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, download_limit_);
  if (upload_limit_ > 0)
    curl_easy_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, upload_limit_);
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = '\0';
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  CurlSlist header_list;
  for (const auto &h : headers) {
    header_list.append(h);
  }
  header_list.append("User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list.get());
  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_off_t dl = 0;
  curl_off_t ul = 0;
  curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &dl);
  curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD_T, &ul);
  total_downloaded_ += dl;
  total_uploaded_ += ul;
  if (max_download_ > 0 && total_downloaded_ > max_download_) {
    github_client_log()->error("Maximum download exceeded");
    throw std::runtime_error("Maximum download exceeded");
  }
  if (max_upload_ > 0 && total_uploaded_ > max_upload_) {
    github_client_log()->error("Maximum upload exceeded");
    throw std::runtime_error("Maximum upload exceeded");
  }
  if (res != CURLE_OK) {
    std::string msg = format_curl_error("PATCH", url, res, errbuf);
    github_client_log()->error(msg);
    throw TransientNetworkError(msg);
  }
  if (http_code < 200 || http_code >= 300) {
    github_client_log()->error("curl PATCH {} failed with HTTP code {}", url,
                               http_code);
    throw HttpStatusError(static_cast<int>(http_code),
                          "curl PATCH failed with HTTP code " +
                              std::to_string(http_code));
  }
  return response;
}

/**
 * Issue a DELETE request.
 */
std::string CurlHttpClient::del(const std::string &url,
                                const std::vector<std::string> &headers) {
  CURL *curl = curl_.get();
  curl_easy_reset(curl);
  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  apply_proxy(curl, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
  if (download_limit_ > 0)
    curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, download_limit_);
  if (upload_limit_ > 0)
    curl_easy_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, upload_limit_);
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = '\0';
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  CurlSlist header_list;
  for (const auto &h : headers) {
    header_list.append(h);
  }
  header_list.append("User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list.get());
  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_off_t dl = 0;
  curl_off_t ul = 0;
  curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &dl);
  curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD_T, &ul);
  total_downloaded_ += dl;
  total_uploaded_ += ul;
  if (max_download_ > 0 && total_downloaded_ > max_download_) {
    github_client_log()->error("Maximum download exceeded");
    throw std::runtime_error("Maximum download exceeded");
  }
  if (max_upload_ > 0 && total_uploaded_ > max_upload_) {
    github_client_log()->error("Maximum upload exceeded");
    throw std::runtime_error("Maximum upload exceeded");
  }
  if (res != CURLE_OK) {
    std::string msg = format_curl_error("DELETE", url, res, errbuf);
    github_client_log()->error(msg);
    throw TransientNetworkError(msg);
  }
  if (http_code < 200 || http_code >= 300) {
    github_client_log()->error("curl DELETE {} failed with HTTP code {}", url,
                               http_code);
    throw HttpStatusError(static_cast<int>(http_code),
                          "curl DELETE failed with HTTP code " +
                              std::to_string(http_code));
  }
  return response;
}

namespace {
/**
 * HTTP client wrapper that retries requests with exponential backoff.
 */
class RetryHttpClient : public agpm::HttpClient {
public:
  /**
   * Construct a retrying HTTP client.
   *
   * @param inner Underlying client performing real requests.
   * @param max_retries Maximum retries for transient failures.
   * @param backoff_ms Base delay in milliseconds for exponential backoff.
   */
  RetryHttpClient(std::unique_ptr<agpm::HttpClient> inner, int max_retries,
                  int backoff_ms)
      : inner_(std::move(inner)), max_retries_(max_retries),
        backoff_ms_(backoff_ms) {}

  /// @copydoc HttpClient::get()
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    return request([&] { return inner_->get(url, headers); });
  }

  /// @copydoc HttpClient::get_with_headers()
  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    return request([&] { return inner_->get_with_headers(url, headers); });
  }

  /// @copydoc HttpClient::put()
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    return request([&] { return inner_->put(url, data, headers); });
  }

  /// @copydoc HttpClient::patch()
  std::string patch(const std::string &url, const std::string &data,
                    const std::vector<std::string> &headers) override {
    return request([&] { return inner_->patch(url, data, headers); });
  }

  /// @copydoc HttpClient::del()
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    return request([&] { return inner_->del(url, headers); });
  }

private:
  /**
   * Execute a request with retry handling.
   */
  template <typename F> auto request(F f) -> decltype(f()) {
    int attempt = 0;
    while (true) {
      try {
        return f();
      } catch (const std::exception &e) {
        if (attempt >= max_retries_ || !is_transient(e))
          throw;
        // Exponential backoff: 2^attempt * backoff_ms between retries.
        std::this_thread::sleep_for(
            std::chrono::milliseconds(backoff_ms_ * (1 << attempt)));
        ++attempt;
      }
    }
  }

  /**
   * Determine whether an exception likely represents a transient failure.
   */
  bool is_transient(const std::exception &e) const {
    // Prefer typed exceptions for clarity.
    if (dynamic_cast<const TransientNetworkError *>(&e)) {
      return true;
    }
    if (auto http_err = dynamic_cast<const HttpStatusError *>(&e)) {
      return http_err->status >= 500 && http_err->status < 600;
    }
    // Fallback: inspect message for legacy errors produced before typed exceptions.
    std::string msg = e.what();
    auto pos = msg.find("HTTP code ");
    if (pos != std::string::npos) {
      try {
        int code = std::stoi(msg.substr(pos + 10));
        return code >= 500 && code < 600;
      } catch (...) {
      }
    }
    if (msg.find("curl ") != std::string::npos || msg.find("Timeout") != std::string::npos ||
        msg.find("timed out") != std::string::npos) {
      return true;
    }
    return false;
  }

  std::unique_ptr<agpm::HttpClient> inner_;
  int max_retries_;
  int backoff_ms_;
};
} // namespace

/**
 * Create a GitHub REST API client with optional caching and filtering.
 */
GitHubClient::GitHubClient(std::vector<std::string> tokens,
                           std::unique_ptr<HttpClient> http,
                           std::unordered_set<std::string> include_repos,
                           std::unordered_set<std::string> exclude_repos,
                           int delay_ms, int timeout_ms, int max_retries,
                           std::string api_base, bool dry_run,
                           std::string cache_file)
    : tokens_(std::move(tokens)),
      http_(std::make_unique<RetryHttpClient>(
          http ? std::move(http) : std::make_unique<CurlHttpClient>(timeout_ms),
          max_retries, 100)),
      include_repos_(std::move(include_repos)),
      exclude_repos_(std::move(exclude_repos)), api_base_(std::move(api_base)),
      dry_run_(dry_run), cache_file_(std::move(cache_file)), delay_ms_(delay_ms) {
  ensure_default_logger();
  std::scoped_lock lock(mutex_);
  load_cache_locked();
}

/**
 * Persist any cached responses when the client is destroyed.
 */
GitHubClient::~GitHubClient() {
  std::scoped_lock lock(mutex_);
  save_cache_locked();
}

/**
 * Perform a GET request leveraging an on-disk cache keyed by URL.
 */
HttpResponse
GitHubClient::get_with_cache_locked(const std::string &url,
                                    const std::vector<std::string> &headers) {
  std::vector<std::string> hdrs = headers;
  auto it = cache_.find(url);
  if (it != cache_.end() && !it->second.etag.empty()) {
    hdrs.push_back("If-None-Match: " + it->second.etag);
  }
  HttpResponse res = http_->get_with_headers(url, hdrs);
  if (res.status_code == 304 && it != cache_.end()) {
    github_client_log()->debug("Cache hit for {}", url);
    return {it->second.body, it->second.headers, 200};
  }
  const auto etag_it = std::find_if(
      res.headers.begin(), res.headers.end(),
      [](const std::string &h) { return h.rfind("ETag:", 0) == 0; });
  if (etag_it != res.headers.end()) {
    std::string etag = etag_it->substr(5);
    if (!etag.empty() && etag[0] == ' ')
      etag.erase(0, 1);
    cache_[url] = {etag, res.body, res.headers};
    save_cache_locked();
  }
  return res;
}

/**
 * Load cached HTTP responses from disk.
 */
void GitHubClient::load_cache_locked() {
  if (cache_file_.empty())
    return;
  std::ifstream in(cache_file_);
  if (!in)
    return;
  nlohmann::json j;
  try {
    in >> j;
    for (auto &[url, entry] : j.items()) {
      CachedResponse c;
      c.etag = entry.value("etag", "");
      c.body = entry.value("body", "");
      c.headers = entry.value("headers", std::vector<std::string>{});
      cache_[url] = std::move(c);
    }
  } catch (...) {
  }
}

/**
 * Serialize cached HTTP responses to disk.
 */
void GitHubClient::save_cache_locked() {
  if (cache_file_.empty())
    return;
  nlohmann::json j;
  for (const auto &[url, c] : cache_) {
    j[url] = {{"etag", c.etag}, {"body", c.body}, {"headers", c.headers}};
  }
  std::ofstream out(cache_file_);
  if (out)
    out << j.dump();
}

/**
 * Update the minimum delay enforced between HTTP requests.
 */
void GitHubClient::set_delay_ms(int delay_ms) {
  std::scoped_lock lock(mutex_);
  delay_ms_ = delay_ms;
}

// Flush cache immediately (thread-safe public API)
void GitHubClient::flush_cache() {
  std::scoped_lock lock(mutex_);
  save_cache_locked();
}

void GitHubClient::set_cache_flush_interval(std::chrono::milliseconds interval) {
  cache_flush_interval_ = interval;
  cache_flusher_cv_.notify_all();
}

/**
 * Determine if a repository passes include/exclude filters.
 */
bool GitHubClient::repo_allowed(const std::string &owner,
                                const std::string &repo) const {
  std::string full = owner + "/" + repo;
  if (!include_repos_.empty() && include_repos_.count(full) == 0) {
    return false;
  }
  if (exclude_repos_.count(full) > 0) {
    return false;
  }
  return true;
}

/// @copydoc GitHubClient::list_repositories
std::vector<std::pair<std::string, std::string>>
GitHubClient::list_repositories() {
  std::vector<std::pair<std::string, std::string>> repos;
  github_client_log()->info("Listing repositories");
  std::string url = api_base_ + "/user/repos?per_page=100";

  while (true) {
    // Build headers and url under lock, then release before network I/O.
    std::vector<std::string> headers;
    {
      std::scoped_lock lock(mutex_);
      if (!tokens_.empty()) {
        size_t ti;
        {
          std::scoped_lock rs_lock(rate_state_mutex_);
          ti = token_index_;
        }
        headers.push_back("Authorization: token " + tokens_[ti]);
      }
      headers.push_back("Accept: application/vnd.github+json");
    }

    enforce_delay();
    HttpResponse res;
    try {
      res = get_with_cache_locked(url, headers);
    } catch (const std::exception &e) {
      github_client_log()->error("HTTP GET failed: {}", e.what());
      break;
    }
    if (handle_rate_limit(res))
      continue;
    if (res.status_code < 200 || res.status_code >= 300) {
      github_client_log()->error("HTTP GET {} failed with HTTP code {}", url,
                                 res.status_code);
      break;
    }
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      github_client_log()->error("Failed to parse repository list: {}",
                                 e.what());
      break;
    }
    for (const auto &item : j) {
      if (!item.contains("name") || !item.contains("owner"))
        continue;
      std::string owner = item["owner"]["login"].get<std::string>();
      std::string name = item["name"].get<std::string>();
      if (repo_allowed(owner, name)) {
        github_client_log()->debug("Found repo {}/{}", owner, name);
        repos.emplace_back(owner, name);
      }
    }
    std::string next_url;
    for (const auto &h : res.headers) {
      if (h.rfind("Link:", 0) == 0) {
        std::string links = h.substr(5);
        std::stringstream ss(links);
        std::string part;
        while (std::getline(ss, part, ',')) {
          if (part.find("rel=\"next\"") != std::string::npos) {
            auto start = part.find('<');
            auto end = part.find('>', start);
            if (start != std::string::npos && end != std::string::npos) {
              next_url = part.substr(start + 1, end - start - 1);
            }
          }
        }
      }
    }
    if (next_url.empty())
      break;
    url = next_url;
  }
  github_client_log()->info("Found {} repositories", repos.size());
  return repos;
}

/// @copydoc GitHubClient::list_pull_requests
std::vector<PullRequest>
GitHubClient::list_pull_requests(const std::string &owner,
                                 const std::string &repo, bool include_merged,
                                 int per_page, std::chrono::seconds since) {
  std::scoped_lock lock(mutex_);
  if (!repo_allowed(owner, repo)) {
    return {};
  }
  int limit = per_page > 0 ? per_page : 50;
  std::string url = api_base_ + "/repos/" + owner + "/" + repo + "/pulls";
  std::string query;
  if (include_merged) {
    query += "state=all";
  }
  if (limit > 0) {
    if (!query.empty())
      query += "&";
    query += "per_page=" + std::to_string(limit);
  }
  if (!query.empty()) {
    url += "?" + query;
  }
  std::vector<std::string> headers;
    if (!tokens_.empty()) {
      size_t ti;
      {
        std::scoped_lock rs_lock(rate_state_mutex_);
        ti = token_index_;
      }
      headers.push_back("Authorization: token " + tokens_[ti]);
    }
  headers.push_back("Accept: application/vnd.github+json");
  auto cutoff = std::chrono::system_clock::now() - since;
  std::vector<PullRequest> prs;
  while (true) {
    enforce_delay();
    HttpResponse res;
    try {
      res = get_with_cache_locked(url, headers);
    } catch (const std::exception &e) {
      github_client_log()->error("HTTP GET failed: {}", e.what());
      break;
    }
    if (handle_rate_limit(res))
      continue;
    if (res.status_code < 200 || res.status_code >= 300) {
      github_client_log()->error("HTTP GET {} failed with HTTP code {}", url,
                                 res.status_code);
      break;
    }
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      github_client_log()->error("Failed to parse pull request list: {}",
                                 e.what());
      auto num_pos = res.body.find("\"number\"");
      auto title_pos = res.body.find("\"title\"");
      if (num_pos != std::string::npos && title_pos != std::string::npos) {
        try {
          auto num_start = res.body.find_first_of("0123456789", num_pos);
          auto num_end = res.body.find_first_not_of("0123456789", num_start);
          int number =
              std::stoi(res.body.substr(num_start, num_end - num_start));
          auto title_start = res.body.find('"', title_pos + 7);
          if (title_start != std::string::npos) {
            ++title_start;
            auto title_end = res.body.find('"', title_start);
            std::string title =
                res.body.substr(title_start, title_end - title_start);
            prs.push_back({number, title, false, owner, repo});
          }
        } catch (...) {
        }
      }
      break;
    }
    for (const auto &item : j) {
      auto read_timestamp =
          [](const nlohmann::json &object,
             const char *field) -> std::optional<std::string> {
        if (object.contains(field) && object[field].is_string()) {
          std::string value = object[field].get<std::string>();
          if (!value.empty()) {
            return value;
          }
        }
        return std::nullopt;
      };
      std::string ts;
      if (auto updated = read_timestamp(item, "updated_at")) {
        ts = *updated;
      } else if (auto created = read_timestamp(item, "created_at")) {
        ts = *created;
      }
      std::tm tm{};
      std::chrono::system_clock::time_point created =
          std::chrono::system_clock::now();
      if (!ts.empty()) {
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
#ifdef _WIN32
        std::time_t t = _mkgmtime(&tm);
#else
        std::time_t t = timegm(&tm);
#endif
        created = std::chrono::system_clock::from_time_t(t);
      }
      if (since.count() > 0 && created < cutoff)
        continue;
      bool merged = item.contains("merged_at") && !item["merged_at"].is_null();
      PullRequest pr{item["number"].get<int>(),
                     item["title"].get<std::string>(), merged, owner, repo};
      prs.push_back(pr);
      if (static_cast<int>(prs.size()) >= limit)
        break;
    }
    if (static_cast<int>(prs.size()) >= limit)
      break;
    std::string next_url;
    for (const auto &h : res.headers) {
      if (h.rfind("Link:", 0) == 0) {
        std::string links = h.substr(5);
        std::stringstream ss(links);
        std::string part;
        while (std::getline(ss, part, ',')) {
          if (part.find("rel=\"next\"") != std::string::npos) {
            auto start = part.find('<');
            auto end = part.find('>', start);
            if (start != std::string::npos && end != std::string::npos) {
              next_url = part.substr(start + 1, end - start - 1);
            }
          }
        }
      }
    }
    if (next_url.empty())
      break;
    url = next_url;
  }
  return prs;
}

/// @copydoc GitHubClient::list_open_pull_requests_single
std::vector<PullRequest>
GitHubClient::list_open_pull_requests_single(const std::string &owner_repo,
                                             int per_page) {
  std::scoped_lock lock(mutex_);
  std::vector<PullRequest> prs;
  auto pos = owner_repo.find('/');
  if (pos == std::string::npos) {
    return prs;
  }
  std::string owner = owner_repo.substr(0, pos);
  std::string repo = owner_repo.substr(pos + 1);
  if (!repo_allowed(owner, repo)) {
    return prs;
  }
  std::string url = api_base_ + "/repos/" + owner + "/" + repo +
                    "/pulls?state=open&per_page=" + std::to_string(per_page);
  std::vector<std::string> headers;
  if (!tokens_.empty()) {
    size_t ti;
    {
      std::scoped_lock rs_lock(rate_state_mutex_);
      ti = token_index_;
    }
    headers.push_back("Authorization: token " + tokens_[ti]);
  }
  headers.push_back("Accept: application/vnd.github+json");
  enforce_delay();
  HttpResponse res;
  try {
    // Intentionally avoid caching/pagination: tests require a single call
    res = http_->get_with_headers(url, headers);
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to fetch open pull requests: {}",
                               e.what());
    return prs;
  }
  try {
    auto j = nlohmann::json::parse(res.body);
    if (!j.is_array()) {
      return prs;
    }
    for (const auto &item : j) {
      if (!item.contains("number") || !item.contains("title"))
        continue;
      PullRequest pr{};
      pr.number = item["number"].get<int>();
      pr.title = item["title"].get<std::string>();
      pr.merged = false;
      pr.owner = owner;
      pr.repo = repo;
      prs.push_back(std::move(pr));
    }
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to parse pull request list: {}",
                               e.what());
  }
  return prs;
}

/// @copydoc GitHubClient::pull_request_metadata
std::optional<PullRequestMetadata>
GitHubClient::pull_request_metadata(const std::string &owner,
                                    const std::string &repo, int pr_number) {
  std::scoped_lock lock(mutex_);
  return pull_request_metadata_locked(owner, repo, pr_number);
}

std::optional<PullRequestMetadata> GitHubClient::pull_request_metadata_locked(
    const std::string &owner, const std::string &repo, int pr_number) {
  if (!repo_allowed(owner, repo)) {
    github_client_log()->debug(
        "Skipping metadata fetch for disallowed repo {}/{}", owner, repo);
    return std::nullopt;
  }
  std::vector<std::string> headers;
  if (!tokens_.empty()) {
    size_t ti;
    {
      std::scoped_lock rs_lock(rate_state_mutex_);
      ti = token_index_;
    }
    headers.push_back("Authorization: token " + tokens_[ti]);
  }
  headers.push_back("Accept: application/vnd.github+json");
  enforce_delay();
  std::string pr_url = api_base_ + "/repos/" + owner + "/" + repo + "/pulls/" +
                       std::to_string(pr_number);
  nlohmann::json meta_json;
  try {
    std::string pr_resp = get_with_cache_locked(pr_url, headers).body;
    meta_json = nlohmann::json::parse(pr_resp);
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to fetch pull request metadata: {}",
                               e.what());
    return std::nullopt;
  }
  if (!meta_json.is_object()) {
    return std::nullopt;
  }
  PullRequestMetadata metadata;
  metadata.approvals = meta_json.value("approvals", 0);
  metadata.mergeable = meta_json.value("mergeable", false);
  if (meta_json.contains("mergeable_state") &&
      meta_json["mergeable_state"].is_string()) {
    metadata.mergeable_state = meta_json["mergeable_state"].get<std::string>();
  }
  if (meta_json.contains("state") && meta_json["state"].is_string()) {
    metadata.state = meta_json["state"].get<std::string>();
  }
  if (meta_json.contains("draft") && meta_json["draft"].is_boolean()) {
    metadata.draft = meta_json["draft"].get<bool>();
  }
  metadata.check_state = interpret_check_state(meta_json);
  return metadata;
}

/// @copydoc GitHubClient::merge_pull_request
bool GitHubClient::merge_pull_request(const std::string &owner,
                                      const std::string &repo, int pr_number) {
  std::scoped_lock lock(mutex_);
  return merge_pull_request_internal(owner, repo, pr_number, nullptr);
}

/// @copydoc GitHubClient::merge_pull_request
bool GitHubClient::merge_pull_request(const std::string &owner,
                                      const std::string &repo, int pr_number,
                                      const PullRequestMetadata &metadata) {
  std::scoped_lock lock(mutex_);
  return merge_pull_request_internal(owner, repo, pr_number, &metadata);
}

bool GitHubClient::merge_pull_request_internal(
    const std::string &owner, const std::string &repo, int pr_number,
    const PullRequestMetadata *metadata) {
  if (!repo_allowed(owner, repo)) {
    github_client_log()->debug("Skipping merge for disallowed repo {}/{}",
                               owner, repo);
    return false;
  }
  github_client_log()->info("Attempting to merge PR #{} in {}/{}", pr_number,
                            owner, repo);
  std::vector<std::string> headers;
  if (!tokens_.empty()) {
    size_t ti;
    {
      std::scoped_lock rs_lock(rate_state_mutex_);
      ti = token_index_;
    }
    headers.push_back("Authorization: token " + tokens_[ti]);
  }
  headers.push_back("Accept: application/vnd.github+json");
  const PullRequestMetadata *meta_ptr = metadata;
  std::optional<PullRequestMetadata> fetched_metadata;
  if (!meta_ptr) {
    auto details = pull_request_metadata_locked(owner, repo, pr_number);
    if (!details) {
      return false;
    }
    fetched_metadata = std::move(details);
    meta_ptr = &*fetched_metadata;
  }
  const PullRequestMetadata &meta = *meta_ptr;
  if (required_approvals_ > 0 && meta.approvals < required_approvals_) {
    github_client_log()->info("PR #{} requires {} approvals but has {}",
                              pr_number, required_approvals_, meta.approvals);
    return false;
  }
  if (require_status_success_ &&
      to_lower_copy(meta.mergeable_state) != "clean") {
    github_client_log()->info("PR #{} status checks not successful", pr_number);
    return false;
  }
  if (require_mergeable_state_ && !meta.mergeable) {
    github_client_log()->info("PR #{} is not mergeable", pr_number);
    return false;
  }
  enforce_delay();
  std::string url = api_base_ + "/repos/" + owner + "/" + repo + "/pulls/" +
                    std::to_string(pr_number) + "/merge";
  if (dry_run_) {
    github_client_log()->info("[dry-run] Would merge PR #{} in {}/{}",
                              pr_number, owner, repo);
    return true;
  }
  try {
    std::string resp = http_->put(url, "{}", headers);
    nlohmann::json j = nlohmann::json::parse(resp);
    bool merged = j.contains("merged") && j["merged"].get<bool>();
    if (merged) {
      github_client_log()->info("Merged PR #{} in {}/{}", pr_number, owner,
                                repo);
    } else {
      github_client_log()->info("PR #{} in {}/{} was not merged", pr_number,
                                owner, repo);
    }
    return merged;
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to merge pull request: {}", e.what());
    return false;
  }
}

bool GitHubClient::close_pull_request(const std::string &owner,
                                      const std::string &repo, int pr_number) {
  std::scoped_lock lock(mutex_);
  if (!repo_allowed(owner, repo)) {
    github_client_log()->debug("Skipping close for disallowed repo {}/{}",
                               owner, repo);
    return false;
  }
  github_client_log()->info("Attempting to close PR #{} in {}/{}", pr_number,
                            owner, repo);
  if (dry_run_) {
    github_client_log()->info("[dry-run] Would close PR #{} in {}/{}",
                              pr_number, owner, repo);
    return true;
  }
  std::vector<std::string> headers;
  if (!tokens_.empty()) {
    size_t ti;
    {
      std::scoped_lock rs_lock(rate_state_mutex_);
      ti = token_index_;
    }
    headers.push_back("Authorization: token " + tokens_[ti]);
  }
  headers.push_back("Accept: application/vnd.github+json");
  headers.push_back("Content-Type: application/json");
  enforce_delay();
  std::string url = api_base_ + "/repos/" + owner + "/" + repo + "/pulls/" +
                    std::to_string(pr_number);
  nlohmann::json payload = {{"state", "closed"}};
  try {
    std::string resp = http_->patch(url, payload.dump(), headers);
    nlohmann::json j = nlohmann::json::parse(resp);
    std::string state = j.value("state", "");
    bool closed = to_lower_copy(state) == "closed";
    if (closed) {
      github_client_log()->info("Closed PR #{} in {}/{}", pr_number, owner,
                                repo);
    } else {
      github_client_log()->info("PR #{} in {}/{} remained {}", pr_number, owner,
                                repo, state);
    }
    return closed;
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to close pull request: {}", e.what());
    return false;
  }
}

bool GitHubClient::delete_branch(
    const std::string &owner, const std::string &repo,
    const std::string &branch,
    const std::vector<std::string> &protected_branches,
    const std::vector<std::string> &protected_branch_excludes) {
  std::scoped_lock lock(mutex_);
  if (!repo_allowed(owner, repo)) {
    github_client_log()->debug(
        "Skipping branch delete for disallowed repo {}/{}", owner, repo);
    return false;
  }
  if (branch.empty()) {
    github_client_log()->warn("Refusing to delete empty branch name in {}/{}",
                              owner, repo);
    return false;
  }
  if (!allow_delete_base_branch_ && is_base_branch_name(branch)) {
    github_client_log()->warn(
        "Refusing to delete protected base branch {} in {}/{}", branch, owner,
        repo);
    return false;
  }
  if (is_protected_branch(branch, protected_branches,
                          protected_branch_excludes)) {
    github_client_log()->warn(
        "Branch {} in {}/{} matches a protected pattern; skipping deletion",
        branch, owner, repo);
    return false;
  }

  std::vector<std::string> headers;
  if (!tokens_.empty()) {
    headers.push_back("Authorization: token " + tokens_[current_token_index()]);
  }
  headers.push_back("Accept: application/vnd.github+json");

  std::string url = api_base_ + "/repos/" + owner + "/" + repo +
                    "/git/refs/heads/" + encode_ref_segment(branch);
  github_client_log()->info("Attempting to delete branch {} from {}/{}", branch,
                            owner, repo);
  if (dry_run_) {
    github_client_log()->info("[dry-run] Would delete branch {} from {}/{}",
                              branch, owner, repo);
    return true;
  }

  enforce_delay();
  try {
    http_->del(url, headers);
    github_client_log()->info("Deleted branch {} from {}/{}", branch, owner,
                              repo);
    return true;
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to delete branch {} in {}/{}: {}",
                               branch, owner, repo, e.what());
    return false;
  }
}

/// @copydoc GitHubClient::list_branches
std::vector<std::string>
GitHubClient::list_branches(const std::string &owner, const std::string &repo,
                            std::string *default_branch_out) {
  std::scoped_lock lock(mutex_);
  std::vector<std::string> branches;
  if (!repo_allowed(owner, repo)) {
    return branches;
  }
  if (default_branch_out) {
    *default_branch_out = std::string{};
  }
  std::vector<std::string> headers;
    if (!tokens_.empty()) {
      size_t ti;
      {
        std::scoped_lock rs_lock(rate_state_mutex_);
        ti = token_index_;
      }
      headers.push_back("Authorization: token " + tokens_[ti]);
    }
  headers.push_back("Accept: application/vnd.github+json");
  enforce_delay();
  std::string repo_url = api_base_ + "/repos/" + owner + "/" + repo;
  std::string repo_resp;
  try {
    repo_resp = get_with_cache_locked(repo_url, headers).body;
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to fetch repo metadata: {}", e.what());
    return branches;
  }
  nlohmann::json repo_json;
  try {
    repo_json = nlohmann::json::parse(repo_resp);
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to parse repo metadata: {}", e.what());
    return branches;
  }
  if (!repo_json.is_object() || !repo_json.contains("default_branch")) {
    return branches;
  }
  std::string default_branch = repo_json["default_branch"].get<std::string>();
  std::string url = repo_url + "/branches";
  while (true) {
    enforce_delay();
    HttpResponse res;
    try {
      res = get_with_cache_locked(url, headers);
    } catch (const std::exception &e) {
      github_client_log()->error("Failed to fetch branches: {}", e.what());
      return branches;
    }
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      github_client_log()->error("Failed to parse branches list: {}", e.what());
      return branches;
    }
    if (!j.is_array()) {
      return branches;
    }
    for (const auto &b : j) {
      if (!b.contains("name")) {
        continue;
      }
      std::string branch = b["name"].get<std::string>();
      if (branch != default_branch) {
        branches.push_back(branch);
      }
    }
    std::string next_url;
    for (const auto &h : res.headers) {
      if (h.rfind("Link:", 0) == 0) {
        std::string links = h.substr(5);
        std::stringstream ss(links);
        std::string part;
        while (std::getline(ss, part, ',')) {
          if (part.find("rel=\"next\"") != std::string::npos) {
            auto start = part.find('<');
            auto end = part.find('>', start);
            if (start != std::string::npos && end != std::string::npos) {
              next_url = part.substr(start + 1, end - start - 1);
            }
          }
        }
      }
    }
    if (next_url.empty()) {
      break;
    }
    url = next_url;
  }
  if (default_branch_out) {
    *default_branch_out = default_branch;
  }
  return branches;
}

std::vector<std::string> GitHubClient::detect_stray_branches(
    const std::string &owner, const std::string &repo,
    const std::string &default_branch, const std::vector<std::string> &branches,
    const std::vector<std::string> &protected_branches,
    const std::vector<std::string> &protected_branch_excludes) {
  std::scoped_lock lock(mutex_);
  std::vector<std::string> stray;
  if (!repo_allowed(owner, repo) || default_branch.empty()) {
    return stray;
  }
  if (branches.empty()) {
    return stray;
  }
  std::vector<std::string> headers;
  if (!tokens_.empty()) {
    size_t ti;
    {
      std::scoped_lock rs_lock(rate_state_mutex_);
      ti = token_index_;
    }
    headers.push_back("Authorization: token " + tokens_[ti]);
  }
  headers.push_back("Accept: application/vnd.github+json");
  const std::string repo_url = api_base_ + "/repos/" + owner + "/" + repo;
  const auto now = std::chrono::system_clock::now();
  constexpr auto kStaleThreshold = std::chrono::hours(24 * 30);
  const std::array<std::string, 5> ephemeral_tokens = {
      "tmp", "wip", "experiment", "backup", "scratch"};
  auto parse_timestamp = [](const std::string &iso)
      -> std::optional<std::chrono::system_clock::time_point> {
    if (iso.empty()) {
      return std::nullopt;
    }
    std::tm tm{};
    std::istringstream ss(iso);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) {
      return std::nullopt;
    }
#ifdef _WIN32
    std::time_t t = _mkgmtime(&tm);
#else
    std::time_t t = timegm(&tm);
#endif
    if (t == static_cast<std::time_t>(-1)) {
      return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(t);
  };
  auto extract_commit_time = [&](const nlohmann::json &entry)
      -> std::optional<std::chrono::system_clock::time_point> {
    if (!entry.is_object()) {
      return std::nullopt;
    }
    if (entry.contains("date") && entry["date"].is_string()) {
      return parse_timestamp(entry["date"].get<std::string>());
    }
    return std::nullopt;
  };
  for (const auto &branch : branches) {
    if (branch.empty() || branch == default_branch) {
      continue;
    }
    if (!allow_delete_base_branch_ && is_base_branch_name(branch)) {
      continue;
    }
    if (is_protected_branch(branch, protected_branches,
                            protected_branch_excludes)) {
      continue;
    }
    int ahead_by = 0;
    int behind_by = 0;
    std::string status;
    try {
      enforce_delay();
      std::string compare_url = repo_url + "/compare/" +
                                encode_ref_segment(default_branch) + "..." +
                                encode_ref_segment(branch);
      std::string compare_resp = http_->get(compare_url, headers);
      nlohmann::json compare_json = nlohmann::json::parse(compare_resp);
      if (compare_json.is_object()) {
        ahead_by = compare_json.value("ahead_by", 0);
        behind_by = compare_json.value("behind_by", 0);
        status = compare_json.value("status", std::string{});
      }
    } catch (const std::exception &e) {
      github_client_log()->debug("Failed to compare branch {}: {}", branch,
                                 e.what());
    }
    bool identical = status == "identical";
    bool behind_only =
        ahead_by == 0 && (status == "behind" || status == "identical");
    bool diverged_without_commits = (status == "diverged" && ahead_by == 0);
    std::optional<std::chrono::system_clock::time_point> last_commit_time;
    try {
      enforce_delay();
      std::string branch_url =
          repo_url + "/branches/" + encode_ref_segment(branch);
      HttpResponse branch_resp = get_with_cache_locked(branch_url, headers);
      nlohmann::json branch_json = nlohmann::json::parse(branch_resp.body);
      if (branch_json.is_object() && branch_json.contains("commit") &&
          branch_json["commit"].is_object()) {
        const auto &commit_wrapper = branch_json["commit"];
        if (commit_wrapper.contains("commit") &&
            commit_wrapper["commit"].is_object()) {
          const auto &commit_details = commit_wrapper["commit"];
          if (commit_details.contains("committer")) {
            last_commit_time = extract_commit_time(commit_details["committer"]);
          }
          if (!last_commit_time && commit_details.contains("author")) {
            last_commit_time = extract_commit_time(commit_details["author"]);
          }
        }
      }
    } catch (const std::exception &e) {
      github_client_log()->debug("Failed to fetch branch metadata for {}: {}",
                                 branch, e.what());
    }
    bool stale = false;
    if (last_commit_time && *last_commit_time < now) {
      stale = now - *last_commit_time > kStaleThreshold;
    }
    bool ephemeral_name = false;
    std::string lower = to_lower_copy(branch);
    for (const auto &token : ephemeral_tokens) {
      if (lower.find(token) != std::string::npos) {
        ephemeral_name = true;
        break;
      }
    }
    bool stale_signal = stale && (ahead_by <= 5 || ephemeral_name);
    if (identical || behind_only || diverged_without_commits || stale_signal) {
      stray.push_back(branch);
    }
  }
  return stray;
}

/// @copydoc GitHubClient::list_branches_single
std::vector<std::string>
GitHubClient::list_branches_single(const std::string &owner_repo,
                                   int per_page) {
  std::vector<std::string> branches;
  auto pos = owner_repo.find('/');
  if (pos == std::string::npos) {
    return branches;
  }
  std::string owner = owner_repo.substr(0, pos);
  std::string repo = owner_repo.substr(pos + 1);
  if (!repo_allowed(owner, repo)) {
    return branches;
  }
  std::string url = api_base_ + "/repos/" + owner + "/" + repo +
                    "/branches?per_page=" + std::to_string(per_page);
  std::vector<std::string> headers;
    if (!tokens_.empty()) {
      size_t ti;
      {
        std::scoped_lock rs_lock(rate_state_mutex_);
        ti = token_index_;
      }
      headers.push_back("Authorization: token " + tokens_[ti]);
    }
  headers.push_back("Accept: application/vnd.github+json");
  enforce_delay();
  HttpResponse res;
  try {
    // Single call, no pagination or extra default_branch metadata
    res = http_->get_with_headers(url, headers);
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to fetch branches: {}", e.what());
    return branches;
  }
  try {
    auto j = nlohmann::json::parse(res.body);
    if (!j.is_array())
      return branches;
    for (const auto &b : j) {
      if (b.contains("name")) {
        branches.push_back(b["name"].get<std::string>());
      }
    }
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to parse branches list: {}", e.what());
  }
  return branches;
}

/// @copydoc GitHubClient::cleanup_branches
std::vector<std::string> GitHubClient::cleanup_branches(
    const std::string &owner, const std::string &repo,
    const std::string &prefix,
    const std::vector<std::string> &protected_branches,
    const std::vector<std::string> &protected_branch_excludes) {
  std::scoped_lock lock(mutex_);
  std::vector<std::string> deleted;
  if (!repo_allowed(owner, repo) || prefix.empty()) {
    github_client_log()->debug("Skipping branch cleanup for {}/{}", owner,
                               repo);
    return deleted;
  }
  if (!allow_delete_base_branch_ && is_base_branch_name(prefix)) {
    github_client_log()->warn(
        "Refusing to delete protected base branch {} in {}/{}", prefix, owner,
        repo);
    return deleted;
  }
  github_client_log()->info("Cleaning up branches in {}/{} with prefix {}",
                            owner, repo, prefix);
  std::string repo_url = api_base_ + "/repos/" + owner + "/" + repo;
  std::string url = repo_url + "/pulls?state=closed";
  std::vector<std::string> headers;
    if (!tokens_.empty()) {
      size_t ti;
      {
        std::scoped_lock rs_lock(rate_state_mutex_);
        ti = token_index_;
      }
      headers.push_back("Authorization: token " + tokens_[ti]);
    }
  headers.push_back("Accept: application/vnd.github+json");
  std::string default_branch;
  if (!allow_delete_base_branch_) {
    try {
      auto repo_res = get_with_cache_locked(repo_url, headers);
      auto repo_json = nlohmann::json::parse(repo_res.body);
      if (repo_json.is_object() && repo_json.contains("default_branch")) {
        default_branch = repo_json["default_branch"].get<std::string>();
      }
    } catch (const std::exception &e) {
      github_client_log()->debug(
          "Failed to retrieve default branch for {}/{}: {}", owner, repo,
          e.what());
    }
  }
  while (true) {
    enforce_delay();
    HttpResponse res;
    try {
      res = get_with_cache_locked(url, headers);
    } catch (const std::exception &e) {
      github_client_log()->error(
          "Failed to fetch pull requests for cleanup: {}", e.what());
      return deleted;
    }
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      github_client_log()->error(
          "Failed to parse pull requests for cleanup: {}", e.what());
      return deleted;
    }
    if (!j.is_array()) {
      return deleted;
    }
    for (const auto &item : j) {
      if (item.contains("head") && item["head"].contains("ref")) {
        std::string branch = item["head"]["ref"].get<std::string>();
        if (branch.rfind(prefix, 0) == 0 &&
            !is_protected_branch(branch, protected_branches,
                                 protected_branch_excludes)) {
          if (!allow_delete_base_branch_ &&
              (!default_branch.empty() && branch == default_branch)) {
            github_client_log()->warn(
                "Skipping deletion of repository default branch {} in {}/{}",
                branch, owner, repo);
            continue;
          }
          if (!allow_delete_base_branch_ && is_base_branch_name(branch)) {
            github_client_log()->warn(
                "Skipping deletion of protected base branch {} in {}/{}",
                branch, owner, repo);
            continue;
          }
          enforce_delay();
          std::string del_url = api_base_ + "/repos/" + owner + "/" + repo +
                                "/git/refs/heads/" + encode_ref_segment(branch);
          if (dry_run_) {
            github_client_log()->info("[dry-run] Would delete branch {}",
                                      branch);
          } else {
            try {
              (void)http_->del(del_url, headers);
              github_client_log()->info("Deleted branch {}", branch);
              deleted.push_back(branch);
            } catch (const std::exception &e) {
              github_client_log()->error("Failed to delete branch {}: {}",
                                         branch, e.what());
            }
          }
        }
      }
    }
    std::string next_url;
    for (const auto &h : res.headers) {
      if (h.rfind("Link:", 0) == 0) {
        std::string links = h.substr(5);
        std::stringstream ss(links);
        std::string part;
        while (std::getline(ss, part, ',')) {
          if (part.find("rel=\"next\"") != std::string::npos) {
            auto start = part.find('<');
            auto end = part.find('>', start);
            if (start != std::string::npos && end != std::string::npos) {
              next_url = part.substr(start + 1, end - start - 1);
            }
          }
        }
      }
    }
    if (next_url.empty())
      break;
    url = next_url;
  }
  return deleted;
}

/// @copydoc GitHubClient::close_dirty_branches
void GitHubClient::close_dirty_branches(
    const std::string &owner, const std::string &repo,
    const std::vector<std::string> &protected_branches,
    const std::vector<std::string> &protected_branch_excludes) {
  std::scoped_lock lock(mutex_);
  if (!repo_allowed(owner, repo)) {
    return;
  }
  std::vector<std::string> headers;
    if (!tokens_.empty()) {
      size_t ti;
      {
        std::scoped_lock rs_lock(rate_state_mutex_);
        ti = token_index_;
      }
      headers.push_back("Authorization: token " + tokens_[ti]);
    }
  headers.push_back("Accept: application/vnd.github+json");

  // Fetch repository metadata to determine the default branch.
  enforce_delay();
  std::string repo_url = api_base_ + "/repos/" + owner + "/" + repo;
  std::string repo_resp;
  try {
    // Repository metadata does not require response headers, so use the plain
    // GET helper instead of `get_with_cache`. Some HttpClient test doubles only
    // implement `get` which previously caused early returns when metadata was
    // requested via `get_with_headers`.
    repo_resp = http_->get(repo_url, headers);
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to fetch repo metadata: {}", e.what());
    return;
  }
  nlohmann::json repo_json;
  try {
    repo_json = nlohmann::json::parse(repo_resp);
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to parse repo metadata: {}", e.what());
    return;
  }
  if (!repo_json.is_object() || !repo_json.contains("default_branch")) {
    return;
  }
  std::string default_branch = repo_json["default_branch"].get<std::string>();

  std::string url = repo_url + "/branches";
  while (true) {
    enforce_delay();
    HttpResponse res;
    try {
      res = get_with_cache_locked(url, headers);
    } catch (const std::exception &e) {
      github_client_log()->error("Failed to fetch branches: {}", e.what());
      return;
    }
    nlohmann::json branches_json;
    try {
      branches_json = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      github_client_log()->error("Failed to parse branches list: {}", e.what());
      return;
    }
    if (!branches_json.is_array()) {
      return;
    }

    for (const auto &b : branches_json) {
      if (!b.contains("name")) {
        continue;
      }
      std::string branch = b["name"].get<std::string>();
      if (!allow_delete_base_branch_ && branch == default_branch) {
        continue;
      }
      if (!allow_delete_base_branch_ && is_base_branch_name(branch)) {
        github_client_log()->warn(
            "Skipping deletion of protected base branch {} in {}/{}", branch,
            owner, repo);
        continue;
      }
      if (is_protected_branch(branch, protected_branches,
                              protected_branch_excludes)) {
        continue;
      }
      // Compare branch with default branch to detect divergence.
      enforce_delay();
      std::string compare_url = repo_url + "/compare/" +
                                encode_ref_segment(default_branch) + "..." +
                                encode_ref_segment(branch);
      std::string compare_resp;
      try {
        // Fetch comparison without caching since headers are unnecessary and
        // some HttpClient test doubles only implement `get`.
        compare_resp = http_->get(compare_url, headers);
      } catch (const std::exception &e) {
        github_client_log()->error("Failed to compare branch {}: {}", branch,
                                   e.what());
        continue;
      }
      nlohmann::json compare_json;
      try {
        compare_json = nlohmann::json::parse(compare_resp);
      } catch (const std::exception &e) {
        github_client_log()->error(
            "Failed to parse compare JSON for branch {}: {}", branch, e.what());
        continue;
      }
      if (!compare_json.is_object()) {
        continue;
      }
      int ahead_by = compare_json.value("ahead_by", 0);
      std::string status = compare_json.value("status", "");
      if (ahead_by > 0 && (status == "ahead" || status == "diverged")) {
        // Branch has unmerged commits; delete it to reject dirty branch.
        enforce_delay();
        std::string del_url =
            repo_url + "/git/refs/heads/" + encode_ref_segment(branch);
        if (dry_run_) {
          github_client_log()->info("[dry-run] Would delete dirty branch {}",
                                    branch);
        } else {
          try {
            (void)http_->del(del_url, headers);
          } catch (const std::exception &e) {
            github_client_log()->error("Failed to delete branch {}: {}", branch,
                                       e.what());
          }
        }
      }
    }
    std::string next_url;
    for (const auto &h : res.headers) {
      if (h.rfind("Link:", 0) == 0) {
        std::string links = h.substr(5);
        std::stringstream ss(links);
        std::string part;
        while (std::getline(ss, part, ',')) {
          if (part.find("rel=\"next\"") != std::string::npos) {
            auto start = part.find('<');
            auto end = part.find('>', start);
            if (start != std::string::npos && end != std::string::npos) {
              next_url = part.substr(start + 1, end - start - 1);
            }
          }
        }
      }
    }
    if (next_url.empty())
      break;
    url = next_url;
  }
}

std::optional<GitHubClient::RateLimitStatus>
GitHubClient::rate_limit_status(int max_attempts) {
  std::scoped_lock lock(mutex_);
  std::vector<std::string> headers;
  if (!tokens_.empty()) {
    headers.push_back("Authorization: token " + tokens_[current_token_index()]);
  }
  headers.push_back("Accept: application/vnd.github+json");
  std::string url = api_base_ + "/rate_limit";
  int attempts = std::max(1, max_attempts);
  for (int attempt = 0; attempt < attempts; ++attempt) {
    enforce_delay();
    HttpResponse res;
    try {
      res = http_->get_with_headers(url, headers);
    } catch (const std::exception &e) {
      github_client_log()->warn("Failed to query rate limit: {}", e.what());
      return std::nullopt;
    }
    if (handle_rate_limit(res)) {
      continue;
    }
    if (res.status_code < 200 || res.status_code >= 300) {
      github_client_log()->warn("Rate limit endpoint returned HTTP {}",
                                res.status_code);
      return std::nullopt;
    }
    try {
      nlohmann::json j = nlohmann::json::parse(res.body);
      const nlohmann::json *core = nullptr;
      if (j.contains("resources") && j["resources"].is_object()) {
        const auto &resources = j["resources"];
        if (resources.contains("core") && resources["core"].is_object()) {
          core = &resources["core"];
        }
      }
      if (!core && j.contains("rate") && j["rate"].is_object()) {
        core = &j["rate"];
      }
      if (!core) {
        github_client_log()->warn("Unexpected rate limit payload");
        return std::nullopt;
      }
      RateLimitStatus status;
      status.limit = core->value("limit", 0L);
      status.remaining = core->value("remaining", 0L);
      status.used = core->value("used", status.limit - status.remaining);
      long reset_epoch = core->value("reset", 0L);
      if (reset_epoch > 0) {
        auto now = std::chrono::system_clock::now();
        auto reset_time = std::chrono::system_clock::time_point(
            std::chrono::seconds(reset_epoch));
        if (reset_time > now) {
          status.reset_after = std::chrono::duration_cast<std::chrono::seconds>(
              reset_time - now);
        } else {
          status.reset_after = std::chrono::seconds(0);
        }
      }
      return status;
    } catch (const std::exception &e) {
      github_client_log()->warn("Failed to parse rate limit response: {}",
                                e.what());
      return std::nullopt;
    }
  }
  return std::nullopt;
}

/**
 * Inspect response headers for rate limit signals and pause if necessary.
 */
bool GitHubClient::handle_rate_limit(const HttpResponse &resp) {
  long remaining = -1;
  long reset = 0;
  long retry_after = 0;
  for (const auto &h : resp.headers) {
    if (h.rfind("X-RateLimit-Remaining:", 0) == 0) {
      try { remaining = std::stol(h.substr(22)); } catch(...) {}
    } else if (h.rfind("X-RateLimit-Reset:", 0) == 0) {
      try { reset = std::stol(h.substr(19)); } catch(...) {}
    } else if (h.rfind("Retry-After:", 0) == 0) {
      try { retry_after = std::stol(h.substr(12)); } catch(...) {}
    }
  }

  // If multiple tokens are configured, rotate quickly under the rate_state lock
  if ((resp.status_code == 403 || resp.status_code == 429) && tokens_.size() > 1) {
    {
      std::scoped_lock rs_lock(rate_state_mutex_);
      token_index_ = (token_index_ + 1) % tokens_.size();
    }
    github_client_log()->warn("Rate limit hit, switching to next token (index {})", token_index_);
    // Signal caller to retry immediately.
    return true;
  }

  if (resp.status_code == 403 || resp.status_code == 429 || remaining == 0) {
    std::chrono::milliseconds wait{0};
    auto now = std::chrono::system_clock::now();
    if (retry_after > 0) {
      wait = std::chrono::seconds(retry_after);
    } else if (reset > 0) {
      auto reset_time = std::chrono::system_clock::time_point(std::chrono::seconds(reset));
      if (reset_time > now)
        wait = std::chrono::duration_cast<std::chrono::milliseconds>(reset_time - now);
    }
    if (wait.count() > 0) {
      std::this_thread::sleep_for(wait);
    }
    {
      std::scoped_lock rs_lock(rate_state_mutex_);
      rate_state_.last_request = std::chrono::steady_clock::now();
    }
    return true;
  }
  return false;
}

/**
 * Ensure the minimum delay between successive HTTP requests is respected.
 */
void GitHubClient::enforce_delay() {
  if (delay_ms_ <= 0)
    return;
  std::chrono::steady_clock::time_point last;
  {
    std::scoped_lock rs_lock(rate_state_mutex_);
    last = rate_state_.last_request;
  }
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
  if (elapsed < delay_ms_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_ - elapsed));
  }
  {
    std::scoped_lock rs_lock(rate_state_mutex_);
    rate_state_.last_request = std::chrono::steady_clock::now();
  }
}

/// @copydoc GitHubGraphQLClient::GitHubGraphQLClient
GitHubGraphQLClient::GitHubGraphQLClient(std::vector<std::string> tokens,
                                         int timeout_ms, std::string api_base)
    : tokens_(std::move(tokens)), timeout_ms_(timeout_ms),
      api_base_(std::move(api_base)) {}

/// @copydoc GitHubGraphQLClient::list_pull_requests
std::vector<PullRequest>
GitHubGraphQLClient::list_pull_requests(const std::string &owner,
                                        const std::string &repo,
                                        bool include_merged, int per_page) {
  std::vector<PullRequest> prs;
  if (tokens_.empty()) {
    return prs;
  }
  std::string url = api_base_ + "/graphql";
  std::string states = include_merged ? "OPEN,MERGED" : "OPEN";
  std::string query = "query($owner:String!,$name:String!,$first:Int!){"
                      "repository(owner:$owner,";
  query += "name:$name){pullRequests(states:[" + states +
           "],first:$first,orderBy:{field:UPDATED_AT,direction:DESC})";
  query += "{nodes{number title mergedAt} pageInfo{hasNextPage endCursor}}}}";
  nlohmann::json payload{
      {"query", query},
      {"variables", {{"owner", owner}, {"name", repo}, {"first", per_page}}}};
  std::string data = payload.dump();

  CurlHandle curl;
  std::string response;
  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, data.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, data.size());
  curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "agpm");
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
  CurlSlist headers;
  headers.append("Content-Type: application/json");
  std::string auth = "Authorization: bearer " + tokens_[token_index_];
  headers.append(auth);
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
  CURLcode res = curl_easy_perform(curl.get());
  if (res != CURLE_OK) {
    github_client_log()->error("GraphQL query failed: {}",
                               curl_easy_strerror(res));
    return prs;
  }
  try {
    auto json = nlohmann::json::parse(response);
    auto nodes = json["data"]["repository"]["pullRequests"]["nodes"];
    for (const auto &n : nodes) {
      PullRequest pr{};
      pr.number = n["number"].get<int>();
      pr.title = n["title"].get<std::string>();
      pr.merged = !n["mergedAt"].is_null();
      pr.owner = owner;
      pr.repo = repo;
      prs.push_back(std::move(pr));
    }
  } catch (const std::exception &e) {
    github_client_log()->error("Failed to parse GraphQL response: {}",
                               e.what());
  }
  return prs;
}

} // namespace agpm
