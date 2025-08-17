#include "github_client.hpp"
#include "curl/curl.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
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
  struct curl_slist *header_list = nullptr;
  for (const auto &h : headers) {
    header_list = curl_slist_append(header_list, h.c_str());
  }
  header_list =
      curl_slist_append(header_list, "User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(header_list);
  if (res != CURLE_OK) {
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
  struct curl_slist *header_list = nullptr;
  for (const auto &h : headers) {
    header_list = curl_slist_append(header_list, h.c_str());
  }
  header_list =
      curl_slist_append(header_list, "User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(header_list);
  if (res != CURLE_OK) {
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
  struct curl_slist *header_list = nullptr;
  for (const auto &h : headers) {
    header_list = curl_slist_append(header_list, h.c_str());
  }
  header_list =
      curl_slist_append(header_list, "User-Agent: autogithubpullmerge");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(header_list);
  if (res != CURLE_OK) {
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
  std::string resp = http_->get(url, headers);
  nlohmann::json j = nlohmann::json::parse(resp);
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
  std::string resp = http_->put(url, "{}", headers);
  nlohmann::json j = nlohmann::json::parse(resp);
  return j.contains("merged") && j["merged"].get<bool>();
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
  std::string resp = http_->get(url, headers);
  nlohmann::json j = nlohmann::json::parse(resp);
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
        (void)http_->del(del_url, headers);
      }
    }
  }
}

std::vector<Branch> GitHubClient::list_branches(const std::string &owner,
                                                const std::string &repo) {
  std::vector<Branch> branches;
  if (!repo_allowed(repo)) {
    return branches;
  }
  enforce_delay();
  std::string url =
      "https://api.github.com/repos/" + owner + "/" + repo + "/branches";
  std::vector<std::string> headers = {"Authorization: token " + token_,
                                      "Accept: application/vnd.github+json"};
  std::string resp = http_->get(url, headers);
  nlohmann::json j = nlohmann::json::parse(resp, nullptr, false);
  if (!j.is_array()) {
    return branches;
  }
  for (const auto &b : j) {
    if (b.contains("name") && b.contains("commit") &&
        b["commit"].contains("sha")) {
      branches.push_back({b["name"].get<std::string>(),
                          b["commit"]["sha"].get<std::string>()});
    }
  }
  return branches;
}

void GitHubClient::close_dirty_branches(const std::string &owner,
                                        const std::string &repo,
                                        bool delete_dirty) {
  if (!repo_allowed(repo)) {
    return;
  }
  std::vector<std::string> headers = {"Authorization: token " + token_,
                                      "Accept: application/vnd.github+json"};

  enforce_delay();
  std::string repo_url = "https://api.github.com/repos/" + owner + "/" + repo;
  std::string repo_resp = http_->get(repo_url, headers);
  nlohmann::json repo_json = nlohmann::json::parse(repo_resp, nullptr, false);
  if (!repo_json.is_object() || !repo_json.contains("default_branch")) {
    return;
  }
  std::string default_branch = repo_json["default_branch"].get<std::string>();

  auto branches = list_branches(owner, repo);
  std::string default_sha;
  for (const auto &b : branches) {
    if (b.name == default_branch) {
      default_sha = b.sha;
      break;
    }
  }
  if (default_sha.empty()) {
    return;
  }

  for (const auto &b : branches) {
    if (b.name == default_branch) {
      continue;
    }
    if (b.sha != default_sha) {
      if (delete_dirty) {
        enforce_delay();
        std::string del_url = repo_url + "/git/refs/heads/" + b.name;
        (void)http_->del(del_url, headers);
      } else {
        std::cerr << "Branch " << b.name << " diverged from " << default_branch
                  << "\n";
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
