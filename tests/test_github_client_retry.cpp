#include "github_client.hpp"
#include <cassert>
#include <stdexcept>
#include <string>

using namespace agpm;

class FlakyHttpClient : public HttpClient {
public:
  int calls = 0;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    if (calls++ < 2) {
      throw std::runtime_error("curl GET failed with HTTP code 500");
    }
    return "[{\"number\":1,\"title\":\"PR\"}]";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

class BadRequestHttpClient : public HttpClient {
public:
  int calls = 0;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    ++calls;
    throw std::runtime_error("curl GET failed with HTTP code 400");
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

int main() {
  {
    auto http = std::make_unique<FlakyHttpClient>();
    auto *raw = http.get();
    GitHubClient client("token", std::move(http));
    auto prs = client.list_pull_requests("o", "r");
    assert(prs.size() == 1);
    assert(raw->calls == 3);
  }
  {
    auto http = std::make_unique<BadRequestHttpClient>();
    auto *raw = http.get();
    GitHubClient client("token", std::move(http));
    auto prs = client.list_pull_requests("o", "r");
    assert(prs.empty());
    assert(raw->calls == 1);
  }
  return 0;
}
