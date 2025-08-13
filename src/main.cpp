#include "app.hpp"
#include "github_client.hpp"
#include "github_poller.hpp"
#include "tui.hpp"

int main(int argc, char **argv) {
  agpm::App app;
  int ret = app.run(argc, argv);
  if (ret == 0) {
    agpm::GitHubClient client("", nullptr);
    agpm::GitHubPoller poller(client, {}, 1000, 60);
    agpm::Tui ui(client, poller);
    ui.init();
    ui.run();
    ui.cleanup();
  }
  return ret;
}
