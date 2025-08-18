#include "github_client.hpp"
#include "curl/curl.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace agpm {

CurlHandle::CurlHandle() {
  static std::once_flag flag;
  std::call_once(flag, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
  handle_ = curl_easy_init();
  if (!handle_) {
    throw std::runtime_error("Failed to init curl");
  }
}

CurlHandle::~CurlHandle() { curl_easy_cleanup(handle_); }

CurlHttpClient::CurlHttpClient(long timeout_ms) : timeout_ms_(timeout_ms) {}

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

HttpResponse
CurlHttpClient::get_with_headers(const std::string &url,
                                 const std::vector<std::string> &headers) {
  CURL *curl = curl_.get();
  curl_easy_reset(curl);
  std::string response;
  std::vector<std::string> resp_headers;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp_headers);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = '\0';
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  struct curl_slist *header_list = nullptr;
  for (const auto &h : headers) {
    header_list = curl_slist_append(header_list, h.c_str());
  }
  header_list =
      curl_slist_append(header_list, "User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_slist_free_all(header_list);
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
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = '\0';
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  struct curl_slist *header_list = nullptr;
  for (const auto &h : headers) {
    header_list = curl_slist_append(header_list, h.c_str());
  }
  header_list =
      curl_slist_append(header_list, "User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_slist_free_all(header_list);
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
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = '\0';
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  struct curl_slist *header_list = nullptr;
  for (const auto &h : headers) {
    header_list = curl_slist_append(header_list, h.c_str());
  }
  header_list =
      curl_slist_append(header_list, "User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_slist_free_all(header_list);
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

GitHubClient::GitHubClient(std::string token, std::unique_ptr<HttpClient> http,
                           std::vector<std::string> include_repos,
                           std::vector<std::string> exclude_repos, int delay_ms,
                           int timeout_ms, int max_retries)
    : token_(std::move(token)),
      http_(std::make_unique<RetryHttpClient>(
          http ? std::move(http) : std::make_unique<CurlHttpClient>(timeout_ms),
          max_retries, 100)),
      include_repos_(std::move(include_repos)),
      exclude_repos_(std::move(exclude_repos)), delay_ms_(delay_ms),
      last_request_(std::chrono::steady_clock::now() -
                    std::chrono::milliseconds(delay_ms)) {}

void GitHubClient::set_delay_ms(int delay_ms) { delay_ms_ = delay_ms; }

bool GitHubClient::repo_allowed(const std::string &repo) const {
  if (!include_repos_.empty() &&
      std::find(include_repos_.begin(), include_repos_.end(), repo) ==
          include_repos_.end()) {
    return false;
  }
  if (std::find(exclude_repos_.begin(), exclude_repos_.end(), repo) !=
      exclude_repos_.end()) {
    return false;
  }
  return true;
}

std::vector<PullRequest>
GitHubClient::list_pull_requests(const std::string &owner,
                                 const std::string &repo, bool include_merged,
                                 int per_page, std::chrono::seconds since) {
  if (!repo_allowed(repo)) {
    return {};
  }
  int limit = per_page > 0 ? per_page : 50;
  std::string url =
      "https://api.github.com/repos/" + owner + "/" + repo + "/pulls";
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
  std::vector<std::string> headers = {"Authorization: token " + token_,
                                      "Accept: application/vnd.github+json"};
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
      PullRequest pr;
      pr.number = item["number"].get<int>();
      pr.title = item["title"].get<std::string>();
      pr.owner = owner;
      pr.repo = repo;
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
  if (!repo_allowed(repo)) {
    return false;
  }
  enforce_delay();
  std::string url = "https://api.github.com/repos/" + owner + "/" + repo +
                    "/pulls/" + std::to_string(pr_number) + "/merge";
  std::vector<std::string> headers = {"Authorization: token " + token_,
                                      "Accept: application/vnd.github+json"};
  try {
    std::string resp = http_->put(url, "{}", headers);
    nlohmann::json j = nlohmann::json::parse(resp);
    return j.contains("merged") && j["merged"].get<bool>();
  } catch (const std::exception &e) {
    spdlog::error("Failed to merge pull request: {}", e.what());
    return false;
  }
}

void GitHubClient::cleanup_branches(const std::string &owner,
                                    const std::string &repo,
                                    const std::string &prefix) {
  if (!repo_allowed(repo) || prefix.empty()) {
    return;
  }
  std::string url = "https://api.github.com/repos/" + owner + "/" + repo +
                    "/pulls?state=closed";
  std::vector<std::string> headers = {"Authorization: token " + token_,
                                      "Accept: application/vnd.github+json"};
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
        if (branch.rfind(prefix, 0) == 0) {
          enforce_delay();
          std::string del_url = "https://api.github.com/repos/" + owner + "/" +
                                repo + "/git/refs/heads/" + branch;
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

void GitHubClient::close_dirty_branches(const std::string &owner,
                                        const std::string &repo) {
  if (!repo_allowed(repo)) {
    return;
  }
  std::vector<std::string> headers = {"Authorization: token " + token_,
                                      "Accept: application/vnd.github+json"};

  // Fetch repository metadata to determine the default branch.
  enforce_delay();
  std::string repo_url = "https://api.github.com/repos/" + owner + "/" + repo;
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
      if (branch == default_branch) {
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
        try {
          (void)http_->del(del_url, headers);
        } catch (const std::exception &e) {
          spdlog::error("Failed to delete branch {}: {}", branch, e.what());
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

} // namespace agpm
