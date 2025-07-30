#include "tui.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <string>
#include <vector>

namespace agpm {

void render_pr_list() {
  using namespace ftxui;
  // Placeholder pull request data
  std::vector<std::pair<std::string, std::string>> prs = {
      {"#1 Add feature", "open"},
      {"#2 Fix bug", "merged"},
      {"#3 Update docs", "open"}};

  std::vector<Element> rows;
  rows.push_back(hbox(text("Pull Request"), filler(), text("Status")));
  rows.push_back(separator());
  for (const auto &pr : prs) {
    rows.push_back(hbox(text(pr.first), filler(), text(pr.second)));
  }

  auto document = vbox(std::move(rows)) | border;
  auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  screen.Print();
  printf("\n");
}

} // namespace agpm
