#include "mcp_server.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <sstream>
#include <string>
#include <vector>

namespace {

class FakeBackend : public agpm::McpBackend {
public:
  std::vector<std::pair<std::string, std::string>> repositories;
  std::vector<agpm::PullRequest> pull_requests;
  std::vector<std::string> branch_names;
  bool merge_ok{true};
  bool close_ok{true};
  bool delete_ok{true};
  int list_repositories_calls{0};
  int list_pull_requests_calls{0};
  int list_branches_calls{0};
  int merge_calls{0};
  int close_calls{0};
  int delete_calls{0};

  std::vector<std::pair<std::string, std::string>>
  list_repositories() override {
    ++list_repositories_calls;
    return repositories;
  }

  std::vector<agpm::PullRequest>
  list_pull_requests(const std::string &owner, const std::string &repo,
                     bool include_merged) override {
    (void)owner;
    (void)repo;
    (void)include_merged;
    ++list_pull_requests_calls;
    return pull_requests;
  }

  std::vector<std::string> list_branches(const std::string &owner,
                                         const std::string &repo) override {
    (void)owner;
    (void)repo;
    ++list_branches_calls;
    return branch_names;
  }

  bool merge_pull_request(const std::string &owner, const std::string &repo,
                          int pr_number) override {
    (void)owner;
    (void)repo;
    (void)pr_number;
    ++merge_calls;
    return merge_ok;
  }

  bool close_pull_request(const std::string &owner, const std::string &repo,
                          int pr_number) override {
    (void)owner;
    (void)repo;
    (void)pr_number;
    ++close_calls;
    return close_ok;
  }

  bool delete_branch(const std::string &owner, const std::string &repo,
                     const std::string &branch) override {
    (void)owner;
    (void)repo;
    (void)branch;
    ++delete_calls;
    return delete_ok;
  }
};

} // namespace

TEST_CASE("McpServer handles repository listings", "[mcp]") {
  FakeBackend backend;
  backend.repositories = {{"octocat", "hello-world"}};
  agpm::McpServer server(backend);

  nlohmann::json request = nlohmann::json{
      {"jsonrpc", "2.0"}, {"id", 1}, {"method", "listRepositories"}};
  auto response = server.handle_request(request);

  REQUIRE(response.contains("result"));
  const auto &repos = response["result"]["repositories"];
  REQUIRE(repos.is_array());
  REQUIRE(repos.size() == 1);
  REQUIRE(repos[0]["owner"] == "octocat");
  REQUIRE(repos[0]["name"] == "hello-world");
  REQUIRE(backend.list_repositories_calls == 1);
}

TEST_CASE("McpServer supports branch and pull request queries", "[mcp]") {
  FakeBackend backend;
  backend.branch_names = {"feature/foo", "bugfix/bar"};
  backend.pull_requests = {{42, "Improve docs", false, "octocat", "docs"}};
  agpm::McpServer server(backend);

  nlohmann::json list_branches_req = {
      {"jsonrpc", "2.0"},
      {"id", 7},
      {"method", "listBranches"},
      {"params", {{"owner", "octocat"}, {"repo", "hello"}}}};
  auto branch_response = server.handle_request(list_branches_req);
  REQUIRE(branch_response["result"]["branches"].size() == 2);
  REQUIRE(backend.list_branches_calls == 1);

  nlohmann::json list_pr_req = {
      {"jsonrpc", "2.0"},
      {"id", 8},
      {"method", "listPullRequests"},
      {"params",
       {{"owner", "octocat"}, {"repo", "hello"}, {"includeMerged", false}}}};
  auto pr_response = server.handle_request(list_pr_req);
  const auto &prs = pr_response["result"]["pullRequests"];
  REQUIRE(prs.size() == 1);
  REQUIRE(prs[0]["number"] == 42);
  REQUIRE(backend.list_pull_requests_calls == 1);
}

TEST_CASE("McpServer executes mutating operations", "[mcp]") {
  FakeBackend backend;
  agpm::McpServer server(backend);

  // Notification should not emit a response but still execute
  nlohmann::json notify = {
      {"jsonrpc", "2.0"},
      {"method", "mergePullRequest"},
      {"params", {{"owner", "octocat"}, {"repo", "hello"}, {"number", 5}}}};
  auto notify_response = server.handle_request(notify);
  REQUIRE(notify_response.is_null());
  REQUIRE(backend.merge_calls == 1);

  nlohmann::json merge_request = {
      {"jsonrpc", "2.0"},
      {"id", 2},
      {"method", "mergePullRequest"},
      {"params", {{"owner", "octocat"}, {"repo", "hello"}, {"number", 7}}}};
  auto merge_response = server.handle_request(merge_request);
  REQUIRE(merge_response["result"]["merged"].get<bool>());
  REQUIRE(backend.merge_calls == 2);

  backend.delete_ok = false;
  nlohmann::json delete_request = {
      {"jsonrpc", "2.0"},
      {"id", 3},
      {"method", "deleteBranch"},
      {"params",
       {{"owner", "octocat"}, {"repo", "hello"}, {"branch", "feature/foo"}}}};
  auto delete_response = server.handle_request(delete_request);
  REQUIRE(delete_response.contains("error"));
  REQUIRE(backend.delete_calls == 1);
}

TEST_CASE("McpServer run loop emits responses", "[mcp]") {
  FakeBackend backend;
  backend.repositories = {{"octocat", "hello"}};
  agpm::McpServer server(backend);

  std::stringstream input;
  std::stringstream output;
  input << "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"listRepositories\"}\n";
  input << "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}\n";
  server.run(input, output);

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(output, line)) {
    lines.push_back(line);
  }
  REQUIRE(lines.size() == 2);
  auto first = nlohmann::json::parse(lines[0]);
  REQUIRE(first["result"].contains("repositories"));
  auto second = nlohmann::json::parse(lines[1]);
  REQUIRE(second["result"]["acknowledged"].get<bool>());
}

TEST_CASE("McpServer reports events to callbacks", "[mcp]") {
  FakeBackend backend;
  backend.repositories = {{"octocat", "hello"}};
  agpm::McpServer server(backend);
  std::vector<std::string> events;
  server.set_event_callback(
      [&events](const std::string &msg) { events.push_back(msg); });
  nlohmann::json request = {
      {"jsonrpc", "2.0"}, {"id", 42}, {"method", "listRepositories"}};
  auto response = server.handle_request(request);
  REQUIRE(response.contains("result"));
  REQUIRE(!events.empty());
  bool saw_list = false;
  for (const auto &evt : events) {
    if (evt.find("listRepositories") != std::string::npos) {
      saw_list = true;
      break;
    }
  }
  REQUIRE(saw_list);
}
