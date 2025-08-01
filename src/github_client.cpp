#include "github_client.hpp"
#include "curl/curl.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace agpm {

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t total = size * nmemb;
  std::string *s = static_cast<std::string *>(userp);
  s->append(static_cast<char *>(contents), total);
  return total;
}

std::string CurlHttpClient::get(const std::string &url,
                                const std::vector<std::string> &headers) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to init curl");
  }
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
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    throw std::runtime_error("curl GET failed");
  }
  return response;
}

std::string CurlHttpClient::put(const std::string &url, const std::string &data,
                                const std::vector<std::string> &headers) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to init curl");
  }
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
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    throw std::runtime_error("curl PUT failed");
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
                                 int per_page) {
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
  for (const auto &item : j) {
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
