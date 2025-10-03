#include "github_client.hpp"
#include <iostream>

using namespace agpm;

class DummyHttp : public HttpClient {
public:
  std::string get(const std::string &, const std::vector<std::string> &) override {
    return "[{\"number\":1,\"title\":\"Test PR\"}]";
  }
  std::string put(const std::string &, const std::string &, const std::vector<std::string> &) override {
    return "{}";
  }
  std::string del(const std::string &, const std::vector<std::string> &) override {
    return "";
  }
};

int main() {
  auto http = std::make_unique<DummyHttp>();
  GitHubClient client({"tok"}, std::move(http));
  auto prs = client.list_pull_requests("me", "repo");
  std::cout << "count=" << prs.size() << "\n";
  for (auto &pr : prs) {
    std::cout << pr.number << " " << pr.title << "\n";
  }
  return 0;
}
