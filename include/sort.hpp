#ifndef AUTOGITHUBPULLMERGE_SORT_HPP
#define AUTOGITHUBPULLMERGE_SORT_HPP

#include "github_client.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace agpm {

inline bool alphanum_less(const std::string &a, const std::string &b) {
  size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    if (std::isdigit(static_cast<unsigned char>(a[i])) &&
        std::isdigit(static_cast<unsigned char>(b[j]))) {
      size_t i1 = i;
      while (i1 < a.size() && std::isdigit(static_cast<unsigned char>(a[i1])))
        ++i1;
      size_t j1 = j;
      while (j1 < b.size() && std::isdigit(static_cast<unsigned char>(b[j1])))
        ++j1;
      long n1 = std::stol(a.substr(i, i1 - i));
      long n2 = std::stol(b.substr(j, j1 - j));
      if (n1 != n2)
        return n1 < n2;
      i = i1;
      j = j1;
    } else {
      char c1 =
          static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
      char c2 =
          static_cast<char>(std::tolower(static_cast<unsigned char>(b[j])));
      if (c1 != c2)
        return c1 < c2;
      ++i;
      ++j;
    }
  }
  return a.size() < b.size();
}

inline void sort_pull_requests(std::vector<PullRequest> &prs,
                               const std::string &mode) {
  if (mode == "alpha") {
    std::sort(prs.begin(), prs.end(),
              [](const PullRequest &a, const PullRequest &b) {
                return a.title < b.title;
              });
  } else if (mode == "reverse") {
    std::sort(prs.begin(), prs.end(),
              [](const PullRequest &a, const PullRequest &b) {
                return a.title > b.title;
              });
  } else if (mode == "alphanum") {
    std::sort(prs.begin(), prs.end(),
              [](const PullRequest &a, const PullRequest &b) {
                return alphanum_less(a.title, b.title);
              });
  } else if (mode == "reverse-alphanum") {
    std::sort(prs.begin(), prs.end(),
              [](const PullRequest &a, const PullRequest &b) {
                return alphanum_less(b.title, a.title);
              });
  }
}

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_SORT_HPP
