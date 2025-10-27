#include "mcp_server.hpp"
#include "log.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace agpm {

namespace {
std::shared_ptr<spdlog::logger> mcp_log() {
  static auto logger = [] {
    ensure_default_logger();
    return category_logger("mcp.server");
  }();
  return logger;
}

nlohmann::json default_initialize_response() {
  return nlohmann::json{
      {"protocolVersion", "0.1"},
      {"capabilities",
       {{"repositories", true}, {"pullRequests", true}, {"branches", true}}}};
}
} // namespace

GitHubMcpBackend::GitHubMcpBackend(
    GitHubClient &client,
    std::vector<std::pair<std::string, std::string>> repositories,
    std::vector<std::string> protected_branches,
    std::vector<std::string> protected_branch_excludes)
    : client_(client), repositories_(std::move(repositories)),
      protected_branches_(std::move(protected_branches)),
      protected_branch_excludes_(std::move(protected_branch_excludes)) {}

std::vector<std::pair<std::string, std::string>>
GitHubMcpBackend::list_repositories() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!repositories_.empty()) {
    return repositories_;
  }
  return client_.list_repositories();
}

std::vector<PullRequest> GitHubMcpBackend::list_pull_requests(
    const std::string &owner, const std::string &repo, bool include_merged) {
  std::lock_guard<std::mutex> lock(mutex_);
  return client_.list_pull_requests(owner, repo, include_merged);
}

std::vector<std::string>
GitHubMcpBackend::list_branches(const std::string &owner,
                                const std::string &repo) {
  std::lock_guard<std::mutex> lock(mutex_);
  return client_.list_branches(owner, repo);
}

bool GitHubMcpBackend::merge_pull_request(const std::string &owner,
                                          const std::string &repo,
                                          int pr_number) {
  std::lock_guard<std::mutex> lock(mutex_);
  return client_.merge_pull_request(owner, repo, pr_number);
}

bool GitHubMcpBackend::close_pull_request(const std::string &owner,
                                          const std::string &repo,
                                          int pr_number) {
  std::lock_guard<std::mutex> lock(mutex_);
  return client_.close_pull_request(owner, repo, pr_number);
}

bool GitHubMcpBackend::delete_branch(const std::string &owner,
                                     const std::string &repo,
                                     const std::string &branch) {
  std::lock_guard<std::mutex> lock(mutex_);
  return client_.delete_branch(owner, repo, branch, protected_branches_,
                               protected_branch_excludes_);
}

McpServer::McpServer(McpBackend &backend) : backend_(backend) {}

nlohmann::json McpServer::make_error(const nlohmann::json &id, int code,
                                     const std::string &message) const {
  return nlohmann::json{{"jsonrpc", "2.0"},
                        {"id", id},
                        {"error", {{"code", code}, {"message", message}}}};
}

nlohmann::json McpServer::make_result(const nlohmann::json &id,
                                      const nlohmann::json &result) const {
  return nlohmann::json{{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

nlohmann::json McpServer::handle_request(const nlohmann::json &request) {
  if (!request.is_object()) {
    emit_event("reject: request not an object");
    return make_error(nullptr, -32600, "Invalid request object");
  }
  bool has_id = request.contains("id");
  nlohmann::json id = has_id ? request["id"] : nullptr;

  auto method_it = request.find("method");
  if (method_it == request.end() || !method_it->is_string()) {
    emit_event("reject: missing method");
    if (!has_id) {
      mcp_log()->warn("Missing method name");
      return nlohmann::json{};
    }
    return make_error(id, -32600, "Missing method name");
  }
  std::string method = method_it->get<std::string>();

  auto respond_error = [&](int code, const std::string &message) {
    emit_event("method=" + method + " error(" + std::to_string(code) +
               "): " + message);
    if (!has_id) {
      mcp_log()->warn("{}", message);
      return nlohmann::json{};
    }
    return make_error(id, code, message);
  };

  nlohmann::json params = nlohmann::json::object();
  auto params_it = request.find("params");
  if (params_it != request.end()) {
    if (!params_it->is_object()) {
      return respond_error(-32602, "Parameters must be an object");
    }
    params = *params_it;
  }

  try {
    if (method == "initialize") {
      if (!has_id) {
        emit_event("method=initialize notification ignored");
        return nlohmann::json{};
      }
      emit_event("method=initialize ok");
      return make_result(id, default_initialize_response());
    }
    if (method == "ping") {
      if (!has_id) {
        emit_event("method=ping notification");
        return nlohmann::json{};
      }
      emit_event("method=ping ok");
      return make_result(id, nlohmann::json{{"message", "pong"}});
    }
    if (method == "shutdown") {
      running_ = false;
      if (!has_id) {
        emit_event("method=shutdown acknowledged (notification)");
        return nlohmann::json{};
      }
      emit_event("method=shutdown acknowledged");
      return make_result(id, nlohmann::json{{"acknowledged", true}});
    }
    if (method == "listRepositories") {
      auto repos = backend_.list_repositories();
      nlohmann::json result = nlohmann::json::array();
      result.reserve(repos.size());
      for (const auto &repo : repos) {
        result.push_back({{"owner", repo.first},
                          {"name", repo.second},
                          {"slug", repo.first + "/" + repo.second}});
      }
      emit_event("method=listRepositories count=" +
                 std::to_string(result.size()));
      if (!has_id) {
        return nlohmann::json{};
      }
      return make_result(id, nlohmann::json{{"repositories", result}});
    }
    if (method == "listBranches") {
      auto owner_it = params.find("owner");
      auto repo_it = params.find("repo");
      if (owner_it == params.end() || !owner_it->is_string() ||
          repo_it == params.end() || !repo_it->is_string()) {
        return respond_error(-32602, "owner and repo must be strings");
      }
      auto branches = backend_.list_branches(owner_it->get<std::string>(),
                                             repo_it->get<std::string>());
      emit_event("method=listBranches count=" +
                 std::to_string(branches.size()));
      if (!has_id) {
        return nlohmann::json{};
      }
      return make_result(id, nlohmann::json{{"branches", branches}});
    }
    if (method == "listPullRequests") {
      auto owner_it = params.find("owner");
      auto repo_it = params.find("repo");
      if (owner_it == params.end() || !owner_it->is_string() ||
          repo_it == params.end() || !repo_it->is_string()) {
        return respond_error(-32602, "owner and repo must be strings");
      }
      bool include_merged = false;
      auto include_it = params.find("includeMerged");
      if (include_it != params.end()) {
        if (!include_it->is_boolean()) {
          return respond_error(-32602, "includeMerged must be a boolean");
        }
        include_merged = include_it->get<bool>();
      }
      auto prs = backend_.list_pull_requests(owner_it->get<std::string>(),
                                             repo_it->get<std::string>(),
                                             include_merged);
      nlohmann::json result = nlohmann::json::array();
      result.reserve(prs.size());
      for (const auto &pr : prs) {
        result.push_back({{"number", pr.number},
                          {"title", pr.title},
                          {"merged", pr.merged},
                          {"owner", pr.owner},
                          {"repo", pr.repo}});
      }
      emit_event("method=listPullRequests count=" +
                 std::to_string(result.size()));
      if (!has_id) {
        return nlohmann::json{};
      }
      return make_result(id, nlohmann::json{{"pullRequests", result}});
    }
    if (method == "mergePullRequest" || method == "closePullRequest" ||
        method == "deleteBranch") {
      auto owner_it = params.find("owner");
      auto repo_it = params.find("repo");
      if (owner_it == params.end() || !owner_it->is_string() ||
          repo_it == params.end() || !repo_it->is_string()) {
        return respond_error(-32602, "owner and repo must be strings");
      }
      const std::string owner = owner_it->get<std::string>();
      const std::string repo = repo_it->get<std::string>();

      if (method == "mergePullRequest" || method == "closePullRequest") {
        auto number_it = params.find("number");
        if (number_it == params.end() || !(number_it->is_number_integer() ||
                                           number_it->is_number_unsigned())) {
          return respond_error(-32602, "number must be an integer");
        }
        int number = number_it->get<int>();
        bool ok = method == "mergePullRequest"
                      ? backend_.merge_pull_request(owner, repo, number)
                      : backend_.close_pull_request(owner, repo, number);
        if (!ok) {
          return respond_error(-32001, method == "mergePullRequest"
                                           ? "Merge rejected by backend"
                                           : "Close rejected by backend");
        }
        emit_event("method=" + method + " success owner=" + owner +
                   " repo=" + repo + " number=" + std::to_string(number));
        if (!has_id) {
          return nlohmann::json{};
        }
        if (method == "mergePullRequest") {
          return make_result(id, nlohmann::json{{"merged", true}});
        }
        return make_result(id, nlohmann::json{{"closed", true}});
      }

      auto branch_it = params.find("branch");
      if (branch_it == params.end() || !branch_it->is_string()) {
        return respond_error(-32602, "branch must be a string");
      }
      std::string branch = branch_it->get<std::string>();
      bool ok = backend_.delete_branch(owner, repo, branch);
      if (!ok) {
        return respond_error(-32002, "Branch deletion rejected by backend");
      }
      emit_event("method=deleteBranch success owner=" + owner + " repo=" +
                 repo + " branch=" + branch);
      if (!has_id) {
        return nlohmann::json{};
      }
      return make_result(id, nlohmann::json{{"deleted", true}});
    }
    emit_event("method=" + method + " error(-32601): Method not found");
    return respond_error(-32601, "Method not found");
  } catch (const std::exception &e) {
    mcp_log()->error("Exception while handling MCP request: {}", e.what());
    emit_event("method=" + method + " exception: " + e.what());
    return respond_error(-32603, e.what());
  }
}

void McpServer::reset() { running_.store(true); }

bool McpServer::running() const { return running_.load(); }

void McpServer::set_event_callback(EventCallback cb) {
  std::lock_guard<std::mutex> lock(event_mutex_);
  event_callback_ = std::move(cb);
}

void McpServer::emit_event(const std::string &message) const {
  EventCallback callback;
  {
    std::lock_guard<std::mutex> lock(event_mutex_);
    callback = event_callback_;
  }
  if (callback) {
    callback(message);
  }
}

void McpServer::run(std::istream &input, std::ostream &output) {
  reset();
  std::string line;
  while (running_.load() && std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    nlohmann::json response;
    try {
      auto request = nlohmann::json::parse(line);
      response = handle_request(request);
    } catch (const std::exception &e) {
      mcp_log()->error("Failed to parse MCP request: {}", e.what());
      emit_event(std::string("parse error: ") + e.what());
      response = make_error(nullptr, -32700, e.what());
    }
    if (!response.is_null()) {
      output << response.dump() << std::endl;
    }
  }
}

McpServerRunner::McpServerRunner(McpServer &server, McpServerOptions options)
    : server_(server), options_(std::move(options)) {}

McpServerRunner::~McpServerRunner() { stop(); }

void McpServerRunner::set_event_sink(EventSink sink) {
  std::lock_guard<std::mutex> lock(sink_mutex_);
  sink_ = std::move(sink);
}

void McpServerRunner::emit(const std::string &message) {
  mcp_log()->debug("{}", message);
  EventSink sink;
  {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    sink = sink_;
  }
  if (sink) {
    sink(message);
  }
}

void McpServerRunner::start() {
  if (running_) {
    return;
  }
  stop_requested_ = false;
  server_.reset();
  running_ = true;
  thread_ = std::thread([this] { run(); });
}

void McpServerRunner::stop() {
  stop_requested_ = true;
  close_listener();
  if (thread_.joinable()) {
    thread_.join();
  }
  running_ = false;
}

void McpServerRunner::close_listener() {
#ifdef _WIN32
  if (listener_ != INVALID_SOCKET) {
    closesocket(listener_);
    listener_ = INVALID_SOCKET;
  }
#else
  if (listener_ >= 0) {
    ::shutdown(listener_, SHUT_RDWR);
    ::close(listener_);
    listener_ = -1;
  }
#endif
}

void McpServerRunner::run() {
#ifdef _WIN32
  bool wsa_started = false;
  WSADATA wsa_data{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    int err = WSAGetLastError();
    emit("WSAStartup failed: " + std::system_category().message(err));
    running_ = false;
    return;
  }
  wsa_started = true;
#endif

  auto finish = [&](const std::string &message) {
    if (!message.empty()) {
      emit(message);
    }
    close_listener();
#ifdef _WIN32
    if (wsa_started) {
      WSACleanup();
      wsa_started = false;
    }
#endif
    running_ = false;
  };

  auto last_error_code = [] {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
  };

  auto describe_error = [&](int code) {
    return std::system_category().message(code);
  };

#ifdef _WIN32
  listener_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener_ == INVALID_SOCKET) {
    finish("Failed to create MCP socket: " +
           describe_error(last_error_code()));
    return;
  }
#else
  listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listener_ < 0) {
    finish("Failed to create MCP socket: " +
           describe_error(last_error_code()));
    return;
  }
#endif

  int enable = 1;
#ifdef _WIN32
  setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char *>(&enable), sizeof(enable));
#else
  setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#endif

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(options_.port));
  std::string bind_address_desc = options_.bind_address.empty()
                                      ? std::string{"0.0.0.0"}
                                      : options_.bind_address;
  if (options_.bind_address.empty() || options_.bind_address == "*") {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    int rc = inet_pton(AF_INET, options_.bind_address.c_str(), &addr.sin_addr);
    if (rc != 1) {
      emit("Invalid MCP bind address '" + options_.bind_address +
           "'; falling back to 0.0.0.0");
      addr.sin_addr.s_addr = htonl(INADDR_ANY);
      bind_address_desc = "0.0.0.0";
    }
  }

#ifdef _WIN32
  if (bind(listener_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) ==
      SOCKET_ERROR) {
    finish("Failed to bind MCP socket: " + describe_error(last_error_code()));
    return;
  }
  if (listen(listener_, options_.backlog) == SOCKET_ERROR) {
    finish("Failed to listen on MCP socket: " +
           describe_error(last_error_code()));
    return;
  }
#else
  if (::bind(listener_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    finish("Failed to bind MCP socket: " + describe_error(last_error_code()));
    return;
  }
  if (::listen(listener_, options_.backlog) < 0) {
    finish("Failed to listen on MCP socket: " +
           describe_error(last_error_code()));
    return;
  }
#endif

  emit("Listening on " + bind_address_desc + ":" +
       std::to_string(options_.port));
  int handled_clients = 0;
  while (!stop_requested_) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
#ifdef _WIN32
    SOCKET client = accept(listener_, reinterpret_cast<sockaddr *>(&client_addr),
                           &client_len);
    if (client == INVALID_SOCKET) {
      if (stop_requested_) {
        break;
      }
      emit("accept failed: " + describe_error(last_error_code()));
      continue;
    }
#else
    int client = ::accept(listener_, reinterpret_cast<sockaddr *>(&client_addr),
                          &client_len);
    if (client < 0) {
      if (stop_requested_) {
        break;
      }
      emit("accept failed: " + describe_error(last_error_code()));
      continue;
    }
#endif
    std::array<char, INET_ADDRSTRLEN> addr_buf{};
#ifdef _WIN32
    const char *remote = InetNtopA(AF_INET, &client_addr.sin_addr,
                                   addr_buf.data(), addr_buf.size());
#else
    const char *remote = inet_ntop(AF_INET, &client_addr.sin_addr,
                                   addr_buf.data(), addr_buf.size());
#endif
    std::string remote_str = remote != nullptr ? remote : std::string{"unknown"};
    emit("client connected: " + remote_str + ":" +
         std::to_string(ntohs(client_addr.sin_port)));

    std::string buffer;
    buffer.reserve(4096);
    bool client_active = true;
    while (client_active && !stop_requested_) {
      std::array<char, 4096> chunk{};
#ifdef _WIN32
      int received = recv(client, chunk.data(),
                          static_cast<int>(chunk.size()), 0);
#else
      ssize_t received = ::recv(client, chunk.data(), chunk.size(), 0);
#endif
      if (received <= 0) {
        break;
      }
      buffer.append(chunk.data(), static_cast<std::size_t>(received));
      std::size_t newline_pos = 0;
      while ((newline_pos = buffer.find('\n')) != std::string::npos) {
        std::string payload = buffer.substr(0, newline_pos);
        buffer.erase(0, newline_pos + 1);
        if (!payload.empty() && payload.back() == '\r') {
          payload.pop_back();
        }
        if (payload.empty()) {
          continue;
        }
        emit("request: " + payload);
        nlohmann::json response;
        try {
          auto request = nlohmann::json::parse(payload);
          response = server_.handle_request(request);
        } catch (const std::exception &e) {
          emit(std::string("parse error: ") + e.what());
          response = server_.make_error(nullptr, -32700, e.what());
        }
        if (!response.is_null()) {
          std::string serialized = response.dump();
          emit("response: " + serialized);
          serialized.push_back('\n');
          const char *data = serialized.data();
          std::size_t remaining = serialized.size();
          while (remaining > 0) {
#ifdef _WIN32
            int sent = send(client, data, static_cast<int>(remaining), 0);
#else
            ssize_t sent = ::send(client, data, remaining, 0);
#endif
            if (sent <= 0) {
              client_active = false;
              break;
            }
            data += sent;
            remaining -= static_cast<std::size_t>(sent);
          }
        }
        if (!server_.running()) {
          stop_requested_ = true;
          client_active = false;
          break;
        }
      }
    }
#ifdef _WIN32
    closesocket(client);
#else
    ::close(client);
#endif
    emit("client disconnected");
    ++handled_clients;
    if (options_.max_clients > 0 && handled_clients >= options_.max_clients) {
      emit("Maximum client budget reached; stopping MCP listener");
      break;
    }
  }

  finish("MCP server listener stopped");
}

} // namespace agpm
