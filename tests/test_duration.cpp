#include "util/duration.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace agpm;
using namespace std::chrono;

TEST_CASE("parse_duration supports combined units") {
  CHECK(parse_duration("1h30m") == seconds{3600 + 30 * 60});
  CHECK(parse_duration("2d3h4m5s") ==
        seconds{2 * 86400 + 3 * 3600 + 4 * 60 + 5});
  CHECK(parse_duration("10") == seconds{10});
  CHECK(parse_duration("") == seconds{0});
}

TEST_CASE("parse_duration rejects invalid strings") {
  CHECK_THROWS_AS(parse_duration("1h30"), std::runtime_error);
  CHECK_THROWS_AS(parse_duration("10m5"), std::runtime_error);
  CHECK_THROWS_AS(parse_duration("abc"), std::runtime_error);
  CHECK_THROWS_AS(parse_duration("1.5h"), std::runtime_error);
}
