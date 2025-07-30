#include "github_client.hpp"
#include <curl/curl.h>
#include <stdexcept>

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
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(header_list);
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    throw std::runtime_error("curl PUT failed");
  }
  return response;
}

GitHubClient::GitHubClient(std::string token, std::unique_ptr<HttpClient> http)
    : token_(std::move(token)),
      http_(http ? std::move(http) : std::make_unique<CurlHttpClient>()) {}

std::vector<PullRequest>
GitHubClient::list_pull_requests(const std::string &owner,
                                 const std::string &repo) {
  std::string url =
      "https://api.github.com/repos/" + owner + "/" + repo + "/pulls";
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
  std::string url = "https://api.github.com/repos/" + owner + "/" + repo +
                    "/pulls/" + std::to_string(pr_number) + "/merge";
  std::vector<std::string> headers = {"Authorization: token " + token_,
                                      "Accept: application/vnd.github+json"};
  std::string resp = http_->put(url, "{}", headers);
  nlohmann::json j = nlohmann::json::parse(resp);
  return j.contains("merged") && j["merged"].get<bool>();
}

} // namespace agpm
