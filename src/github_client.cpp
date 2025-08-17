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

CurlHttpClient::CurlHttpClient() = default;

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t total = size * nmemb;
  std::string *s = static_cast<std::string *>(userp);
  s->append(static_cast<char *>(contents), total);
  return total;
}

std::string CurlHttpClient::get(const std::string &url,
                                const std::vector<std::string> &headers) {
  CURL *curl = curl_.get();
  curl_easy_reset(curl);
  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
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
  if (res != CURLE_OK || http_code < 200 || http_code >= 300) {
    spdlog::error("curl GET {} failed with HTTP code {}", url, http_code);
    throw std::runtime_error("curl GET failed");
  }
  return response;
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
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
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
  if (res != CURLE_OK || http_code < 200 || http_code >= 300) {
    spdlog::error("curl PUT {} failed with HTTP code {}", url, http_code);
    throw std::runtime_error("curl PUT failed");
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
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
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
  if (res != CURLE_OK || http_code < 200 || http_code >= 300) {
    spdlog::error("curl DELETE {} failed with HTTP code {}", url, http_code);
    throw std::runtime_error("curl DELETE failed");
  }
  return response;
}

GitHubClient::GitHubClient(std::string token, std::unique_ptr<HttpClient> http,
                           std::vector<std::string> include_repos,
                           std::vector<std::string> exclude_repos, int delay_ms)
    : token_(std::move(token)),
      http_(http ? std::move(http) : std::make_unique<CurlHttpClient>()),
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
  enforce_delay();
  std::string url =
      "https://api.github.com/repos/" + owner + "/" + repo + "/pulls";
  std::string query;
  if (include_merged) {
    query += "state=all";
  }
  if (per_page > 0) {
    if (!query.empty())
      query += "&";
    query += "per_page=" + std::to_string(per_page);
  }
  if (!query.empty()) {
    url += "?" + query;
  }
  std::vector<std::string> headers = {"Authorization: token " + token_,
                                      "Accept: application/vnd.github+json"};
  std::string resp;
  try {
    resp = http_->get(url, headers);
  } catch (const std::exception &e) {
    spdlog::error("HTTP GET failed: {}", e.what());
    return {};
  }
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(resp);
  } catch (const std::exception &e) {
    spdlog::error("Failed to parse pull request list: {}", e.what());
    return {};
  }
  std::vector<PullRequest> prs;
  auto cutoff = std::chrono::system_clock::now() - since;
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
    prs.push_back(pr);
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
  enforce_delay();
  std::string url = "https://api.github.com/repos/" + owner + "/" + repo +
                    "/pulls?state=closed";
  std::vector<std::string> headers = {"Authorization: token " + token_,
                                      "Accept: application/vnd.github+json"};
  std::string resp;
  try {
    resp = http_->get(url, headers);
  } catch (const std::exception &e) {
    spdlog::error("Failed to fetch pull requests for cleanup: {}", e.what());
    return;
  }
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(resp);
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

  // Retrieve branches for the repository.
  enforce_delay();
  std::string branches_url = repo_url + "/branches";
  std::string branches_resp;
  try {
    branches_resp = http_->get(branches_url, headers);
  } catch (const std::exception &e) {
    spdlog::error("Failed to fetch branches: {}", e.what());
    return;
  }
  nlohmann::json branches_json;
  try {
    branches_json = nlohmann::json::parse(branches_resp);
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
