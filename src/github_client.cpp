#include "github_client.hpp"
#include "curl/curl.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <regex>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace agpm {

namespace {

/// Convert a shell-style glob pattern to a std::regex for comparison.
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

/// Return true if `name` matches any glob or regex in `patterns`.
bool matches_pattern(const std::string &name,
                     const std::vector<std::string> &patterns) {
  for (const auto &p : patterns) {
    try {
      if (p.find_first_of("*?") != std::string::npos) {
        if (std::regex_match(name, glob_to_regex(p))) {
          return true;
        }
      } else {
        if (std::regex_match(name, std::regex(p))) {
          return true;
        }
      }
    } catch (const std::regex_error &) {
      if (name == p) {
        return true;
      }
    }
  }
  return false;
}

// Determine whether a branch name should be considered protected based on
// include and exclude pattern lists. Excludes take precedence.
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

} // namespace

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

CurlHandle::CurlHandle() {
  static std::once_flag flag;
  std::call_once(flag, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
  handle_ = curl_easy_init();
  if (!handle_) {
    throw std::runtime_error("Failed to init curl");
  }
}

CurlHandle::~CurlHandle() { curl_easy_cleanup(handle_); }

CurlHttpClient::CurlHttpClient(long timeout_ms, curl_off_t download_limit,
                               curl_off_t upload_limit, curl_off_t max_download,
                               curl_off_t max_upload, std::string http_proxy,
                               std::string https_proxy)
    : timeout_ms_(timeout_ms), download_limit_(download_limit),
      upload_limit_(upload_limit), max_download_(max_download),
      max_upload_(max_upload), http_proxy_(std::move(http_proxy)),
      https_proxy_(std::move(https_proxy)) {}

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t total = size * nmemb;
  std::string *s = static_cast<std::string *>(userp);
  s->append(static_cast<char *>(contents), total);
  return total;
}

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
    spdlog::error("Maximum download exceeded");
    throw std::runtime_error("Maximum download exceeded");
  }
  if (max_upload_ > 0 && total_uploaded_ > max_upload_) {
    spdlog::error("Maximum upload exceeded");
    throw std::runtime_error("Maximum upload exceeded");
  }
  if (res != CURLE_OK) {
    std::ostringstream oss;
    oss << "curl GET failed: " << curl_easy_strerror(res);
    if (errbuf[0] != '\0') {
      oss << " - " << errbuf;
    }
    spdlog::error(oss.str());
    throw std::runtime_error(oss.str());
  }
  if (http_code < 200 || http_code >= 300) {
    if (http_code == 403 || http_code == 429) {
      // Let caller handle rate limiting
      return {response, resp_headers, http_code};
    }
    spdlog::error("curl GET {} failed with HTTP code {}", url, http_code);
    throw std::runtime_error("curl GET failed with HTTP code " +
                             std::to_string(http_code));
  }
  return {response, resp_headers, http_code};
}

std::string CurlHttpClient::get(const std::string &url,
                                const std::vector<std::string> &headers) {
  return get_with_headers(url, headers).body;
}

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
    spdlog::error("Maximum download exceeded");
    throw std::runtime_error("Maximum download exceeded");
  }
  if (max_upload_ > 0 && total_uploaded_ > max_upload_) {
    spdlog::error("Maximum upload exceeded");
    throw std::runtime_error("Maximum upload exceeded");
  }
  if (res != CURLE_OK) {
    std::ostringstream oss;
    oss << "curl PUT failed: " << curl_easy_strerror(res);
    if (errbuf[0] != '\0') {
      oss << " - " << errbuf;
    }
    spdlog::error(oss.str());
    throw std::runtime_error(oss.str());
  }
  if (http_code < 200 || http_code >= 300) {
    spdlog::error("curl PUT {} failed with HTTP code {}", url, http_code);
    throw std::runtime_error("curl PUT failed with HTTP code " +
                             std::to_string(http_code));
  }
  return response;
}

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
    spdlog::error("Maximum download exceeded");
    throw std::runtime_error("Maximum download exceeded");
  }
  if (max_upload_ > 0 && total_uploaded_ > max_upload_) {
    spdlog::error("Maximum upload exceeded");
    throw std::runtime_error("Maximum upload exceeded");
  }
  if (res != CURLE_OK) {
    std::ostringstream oss;
    oss << "curl DELETE failed: " << curl_easy_strerror(res);
    if (errbuf[0] != '\0') {
      oss << " - " << errbuf;
    }
    spdlog::error(oss.str());
    throw std::runtime_error(oss.str());
  }
  if (http_code < 200 || http_code >= 300) {
    spdlog::error("curl DELETE {} failed with HTTP code {}", url, http_code);
    throw std::runtime_error("curl DELETE failed with HTTP code " +
                             std::to_string(http_code));
  }
  return response;
}

namespace {
// HTTP client wrapper that retries requests with exponential backoff on
// transient failures. Uses the wrapped `HttpClient` to perform actual
// requests.
class RetryHttpClient : public agpm::HttpClient {
public:
  RetryHttpClient(std::unique_ptr<agpm::HttpClient> inner, int max_retries,
                  int backoff_ms)
      : inner_(std::move(inner)), max_retries_(max_retries),
        backoff_ms_(backoff_ms) {}

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    return request([&] { return inner_->get(url, headers); });
  }

  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    return request([&] { return inner_->get_with_headers(url, headers); });
  }

  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    return request([&] { return inner_->put(url, data, headers); });
  }

  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    return request([&] { return inner_->del(url, headers); });
  }

private:
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

  bool is_transient(const std::exception &e) const {
    std::string msg = e.what();
    auto pos = msg.find("HTTP code ");
    if (pos != std::string::npos) {
      int code = std::stoi(msg.substr(pos + 10));
      return code >= 500 && code < 600;
    }
    return true;
  }

  std::unique_ptr<agpm::HttpClient> inner_;
  int max_retries_;
  int backoff_ms_;
};
} // namespace

GitHubClient::GitHubClient(std::vector<std::string> tokens,
                           std::unique_ptr<HttpClient> http,
                           std::unordered_set<std::string> include_repos,
                           std::unordered_set<std::string> exclude_repos,
                           int delay_ms, int timeout_ms, int max_retries,
                           std::string api_base, bool dry_run)
    : tokens_(std::move(tokens)), token_index_(0),
      http_(std::make_unique<RetryHttpClient>(
          http ? std::move(http) : std::make_unique<CurlHttpClient>(timeout_ms),
          max_retries, 100)),
      include_repos_(std::move(include_repos)),
      exclude_repos_(std::move(exclude_repos)), api_base_(std::move(api_base)),
      dry_run_(dry_run), delay_ms_(delay_ms),
      last_request_(std::chrono::steady_clock::now() -
                    std::chrono::milliseconds(delay_ms)) {}

void GitHubClient::set_delay_ms(int delay_ms) { delay_ms_ = delay_ms; }

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

std::vector<std::pair<std::string, std::string>>
GitHubClient::list_repositories() {
  std::vector<std::pair<std::string, std::string>> repos;
  spdlog::info("Listing repositories");
  std::string url = api_base_ + "/user/repos?per_page=100";
  std::vector<std::string> headers;
  if (!tokens_.empty())
    headers.push_back("Authorization: token " + tokens_[token_index_]);
  headers.push_back("Accept: application/vnd.github+json");
  while (true) {
    enforce_delay();
    HttpResponse res;
    try {
      res = http_->get_with_headers(url, headers);
    } catch (const std::exception &e) {
      spdlog::error("HTTP GET failed: {}", e.what());
      break;
    }
    if (handle_rate_limit(res))
      continue;
    if (res.status_code < 200 || res.status_code >= 300) {
      spdlog::error("HTTP GET {} failed with HTTP code {}", url,
                    res.status_code);
      break;
    }
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      spdlog::error("Failed to parse repository list: {}", e.what());
      break;
    }
    for (const auto &item : j) {
      if (!item.contains("name") || !item.contains("owner"))
        continue;
      std::string owner = item["owner"]["login"].get<std::string>();
      std::string name = item["name"].get<std::string>();
      if (repo_allowed(owner, name)) {
        spdlog::debug("Found repo {}/{}", owner, name);
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
  spdlog::info("Found {} repositories", repos.size());
  return repos;
}

std::vector<PullRequest>
GitHubClient::list_pull_requests(const std::string &owner,
                                 const std::string &repo, bool include_merged,
                                 int per_page, std::chrono::seconds since) {
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
  if (!tokens_.empty())
    headers.push_back("Authorization: token " + tokens_[token_index_]);
  headers.push_back("Accept: application/vnd.github+json");
  auto cutoff = std::chrono::system_clock::now() - since;
  std::vector<PullRequest> prs;
  while (true) {
    enforce_delay();
    HttpResponse res;
    try {
      res = http_->get_with_headers(url, headers);
    } catch (const std::exception &e) {
      spdlog::error("HTTP GET failed: {}", e.what());
      break;
    }
    if (handle_rate_limit(res))
      continue;
    if (res.status_code < 200 || res.status_code >= 300) {
      spdlog::error("HTTP GET {} failed with HTTP code {}", url,
                    res.status_code);
      break;
    }
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      spdlog::error("Failed to parse pull request list: {}", e.what());
      break;
    }
    for (const auto &item : j) {
      std::string ts;
      if (item.contains("created_at"))
        ts = item["created_at"].get<std::string>();
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

bool GitHubClient::merge_pull_request(const std::string &owner,
                                      const std::string &repo, int pr_number) {
  if (!repo_allowed(owner, repo)) {
    spdlog::debug("Skipping merge for disallowed repo {}/{}", owner, repo);
    return false;
  }
  spdlog::info("Attempting to merge PR #{} in {}/{}", pr_number, owner, repo);
  std::vector<std::string> headers;
  if (!tokens_.empty())
    headers.push_back("Authorization: token " + tokens_[token_index_]);
  headers.push_back("Accept: application/vnd.github+json");
  // Fetch pull request metadata
  enforce_delay();
  std::string pr_url = api_base_ + "/repos/" + owner + "/" + repo + "/pulls/" +
                       std::to_string(pr_number);
  nlohmann::json meta;
  try {
    std::string pr_resp = http_->get(pr_url, headers);
    meta = nlohmann::json::parse(pr_resp);
  } catch (const std::exception &e) {
    spdlog::error("Failed to fetch pull request metadata: {}", e.what());
    return false;
  }
  if (!meta.is_object()) {
    return false;
  }
  int approvals = meta.value("approvals", 0);
  if (required_approvals_ > 0 && approvals < required_approvals_) {
    spdlog::info("PR #{} requires {} approvals but has {}", pr_number,
                 required_approvals_, approvals);
    return false;
  }
  if (require_status_success_ && meta.value("mergeable_state", "") != "clean") {
    spdlog::info("PR #{} status checks not successful", pr_number);
    return false;
  }
  if (require_mergeable_state_ && !meta.value("mergeable", false)) {
    spdlog::info("PR #{} is not mergeable", pr_number);
    return false;
  }
  enforce_delay();
  std::string url = api_base_ + "/repos/" + owner + "/" + repo + "/pulls/" +
                    std::to_string(pr_number) + "/merge";
  if (dry_run_) {
    spdlog::info("[dry-run] Would merge PR #{} in {}/{}", pr_number, owner,
                 repo);
    return true;
  }
  try {
    std::string resp = http_->put(url, "{}", headers);
    nlohmann::json j = nlohmann::json::parse(resp);
    bool merged = j.contains("merged") && j["merged"].get<bool>();
    if (merged) {
      spdlog::info("Merged PR #{} in {}/{}", pr_number, owner, repo);
    } else {
      spdlog::info("PR #{} in {}/{} was not merged", pr_number, owner, repo);
    }
    return merged;
  } catch (const std::exception &e) {
    spdlog::error("Failed to merge pull request: {}", e.what());
    return false;
  }
}

std::vector<std::string> GitHubClient::list_branches(const std::string &owner,
                                                     const std::string &repo) {
  std::vector<std::string> branches;
  if (!repo_allowed(owner, repo)) {
    return branches;
  }
  std::vector<std::string> headers;
  if (!tokens_.empty())
    headers.push_back("Authorization: token " + tokens_[token_index_]);
  headers.push_back("Accept: application/vnd.github+json");
  enforce_delay();
  std::string repo_url = api_base_ + "/repos/" + owner + "/" + repo;
  std::string repo_resp;
  try {
    repo_resp = http_->get(repo_url, headers);
  } catch (const std::exception &e) {
    spdlog::error("Failed to fetch repo metadata: {}", e.what());
    return branches;
  }
  nlohmann::json repo_json;
  try {
    repo_json = nlohmann::json::parse(repo_resp);
  } catch (const std::exception &e) {
    spdlog::error("Failed to parse repo metadata: {}", e.what());
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
      res = http_->get_with_headers(url, headers);
    } catch (const std::exception &e) {
      spdlog::error("Failed to fetch branches: {}", e.what());
      return branches;
    }
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      spdlog::error("Failed to parse branches list: {}", e.what());
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
  return branches;
}

// Remove merged branches with the specified prefix. The implementation pages
// through closed pull requests and deletes the associated head branch when the
// name matches `prefix` and is not protected by user-specified patterns.
void GitHubClient::cleanup_branches(
    const std::string &owner, const std::string &repo,
    const std::string &prefix,
    const std::vector<std::string> &protected_branches,
    const std::vector<std::string> &protected_branch_excludes) {
  if (!repo_allowed(owner, repo) || prefix.empty()) {
    spdlog::debug("Skipping branch cleanup for {}/{}", owner, repo);
    return;
  }
  spdlog::info("Cleaning up branches in {}/{} with prefix {}", owner, repo,
               prefix);
  std::string url =
      api_base_ + "/repos/" + owner + "/" + repo + "/pulls?state=closed";
  std::vector<std::string> headers;
  if (!tokens_.empty())
    headers.push_back("Authorization: token " + tokens_[token_index_]);
  headers.push_back("Accept: application/vnd.github+json");
  while (true) {
    enforce_delay();
    HttpResponse res;
    try {
      res = http_->get_with_headers(url, headers);
    } catch (const std::exception &e) {
      spdlog::error("Failed to fetch pull requests for cleanup: {}", e.what());
      return;
    }
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      spdlog::error("Failed to parse pull requests for cleanup: {}", e.what());
      return;
    }
    if (!j.is_array()) {
      return;
    }
    for (const auto &item : j) {
      if (item.contains("head") && item["head"].contains("ref")) {
        std::string branch = item["head"]["ref"].get<std::string>();
        if (branch.rfind(prefix, 0) == 0 &&
            !is_protected_branch(branch, protected_branches,
                                 protected_branch_excludes)) {
          enforce_delay();
          std::string del_url = api_base_ + "/repos/" + owner + "/" + repo +
                                "/git/refs/heads/" + branch;
          if (dry_run_) {
            spdlog::info("[dry-run] Would delete branch {}", branch);
          } else {
            try {
              (void)http_->del(del_url, headers);
              spdlog::info("Deleted branch {}", branch);
            } catch (const std::exception &e) {
              spdlog::error("Failed to delete branch {}: {}", branch, e.what());
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
}

// Detect branches that have drifted from the default branch and remove or
// close them if they are not protected. This prevents accumulation of stale
// development branches.
void GitHubClient::close_dirty_branches(
    const std::string &owner, const std::string &repo,
    const std::vector<std::string> &protected_branches,
    const std::vector<std::string> &protected_branch_excludes) {
  if (!repo_allowed(owner, repo)) {
    return;
  }
  std::vector<std::string> headers;
  if (!tokens_.empty())
    headers.push_back("Authorization: token " + tokens_[token_index_]);
  headers.push_back("Accept: application/vnd.github+json");

  // Fetch repository metadata to determine the default branch.
  enforce_delay();
  std::string repo_url = api_base_ + "/repos/" + owner + "/" + repo;
  std::string repo_resp;
  try {
    repo_resp = http_->get(repo_url, headers);
  } catch (const std::exception &e) {
    spdlog::error("Failed to fetch repo metadata: {}", e.what());
    return;
  }
  nlohmann::json repo_json;
  try {
    repo_json = nlohmann::json::parse(repo_resp);
  } catch (const std::exception &e) {
    spdlog::error("Failed to parse repo metadata: {}", e.what());
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
      res = http_->get_with_headers(url, headers);
    } catch (const std::exception &e) {
      spdlog::error("Failed to fetch branches: {}", e.what());
      return;
    }
    nlohmann::json branches_json;
    try {
      branches_json = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
      spdlog::error("Failed to parse branches list: {}", e.what());
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
      if (branch == default_branch ||
          is_protected_branch(branch, protected_branches,
                              protected_branch_excludes)) {
        continue;
      }
      // Compare branch with default branch to detect divergence.
      enforce_delay();
      std::string compare_url =
          repo_url + "/compare/" + default_branch + "..." + branch;
      std::string compare_resp;
      try {
        compare_resp = http_->get(compare_url, headers);
      } catch (const std::exception &e) {
        spdlog::error("Failed to compare branch {}: {}", branch, e.what());
        continue;
      }
      nlohmann::json compare_json;
      try {
        compare_json = nlohmann::json::parse(compare_resp);
      } catch (const std::exception &e) {
        spdlog::error("Failed to parse compare JSON for branch {}: {}", branch,
                      e.what());
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
        std::string del_url = repo_url + "/git/refs/heads/" + branch;
        if (dry_run_) {
          spdlog::info("[dry-run] Would delete dirty branch {}", branch);
        } else {
          try {
            (void)http_->del(del_url, headers);
          } catch (const std::exception &e) {
            spdlog::error("Failed to delete branch {}: {}", branch, e.what());
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

bool GitHubClient::handle_rate_limit(const HttpResponse &resp) {
  long remaining = -1;
  long reset = 0;
  long retry_after = 0;
  for (const auto &h : resp.headers) {
    if (h.rfind("X-RateLimit-Remaining:", 0) == 0) {
      remaining = std::stol(h.substr(22));
    } else if (h.rfind("X-RateLimit-Reset:", 0) == 0) {
      reset = std::stol(h.substr(19));
    } else if (h.rfind("Retry-After:", 0) == 0) {
      retry_after = std::stol(h.substr(12));
    }
  }
  if ((resp.status_code == 403 || resp.status_code == 429) &&
      tokens_.size() > 1) {
    token_index_ = (token_index_ + 1) % tokens_.size();
    spdlog::warn("Rate limit hit, switching to next token (index {})",
                 token_index_);
    last_request_ = std::chrono::steady_clock::now();
    return true;
  }
  if (resp.status_code == 403 || resp.status_code == 429 || remaining == 0) {
    std::chrono::milliseconds wait{0};
    auto now = std::chrono::system_clock::now();
    if (retry_after > 0) {
      wait = std::chrono::seconds(retry_after);
    } else if (reset > 0) {
      auto reset_time =
          std::chrono::system_clock::time_point(std::chrono::seconds(reset));
      if (reset_time > now)
        wait = std::chrono::duration_cast<std::chrono::milliseconds>(
            reset_time - now);
    }
    if (wait.count() > 0) {
      std::this_thread::sleep_for(wait);
    }
    last_request_ = std::chrono::steady_clock::now();
    return true;
  }
  return false;
}

void GitHubClient::enforce_delay() {
  if (delay_ms_ <= 0)
    return;
  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_)
          .count();
  if (elapsed < delay_ms_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_ - elapsed));
  }
  last_request_ = std::chrono::steady_clock::now();
}

GitHubGraphQLClient::GitHubGraphQLClient(std::vector<std::string> tokens,
                                         int timeout_ms, std::string api_base)
    : tokens_(std::move(tokens)), timeout_ms_(timeout_ms),
      api_base_(std::move(api_base)) {}

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
    spdlog::error("GraphQL query failed: {}", curl_easy_strerror(res));
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
    spdlog::error("Failed to parse GraphQL response: {}", e.what());
  }
  return prs;
}

} // namespace agpm
