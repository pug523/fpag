// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/matches.h"

#include <string>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"

namespace arg {

TEST_CASE("Matches has() reflects added values", "[arg][matches]") {
  Matches m;
  CHECK_FALSE(m.has("port"));

  m.add("port", "8080");
  CHECK(m.has("port"));
  CHECK_FALSE(m.has("host"));
}

TEST_CASE("Matches get<std::string> and get<std::string_view>",
          "[arg][matches]") {
  Matches m;
  m.add("host", "127.0.0.1");

  auto as_sv = m.get<std::string_view>("host");
  REQUIRE(as_sv.is_ok());
  CHECK(as_sv.unwrap() == "127.0.0.1");

  auto as_str = m.get<std::string>("host");
  REQUIRE(as_str.is_ok());
  CHECK(as_str.unwrap() == "127.0.0.1");
}

TEST_CASE("Matches get<T> on a missing name returns OutOfRange",
          "[arg][matches]") {
  Matches const m;
  auto res = m.get<std::string_view>("missing");
  REQUIRE(res.is_err());
  CHECK(res.unwrap_err() == Matches::GetError::OutOfRange);
}

TEST_CASE("Matches get<i32> parses valid integers", "[arg][matches]") {
  Matches m;
  m.add("port", "9090");

  auto res = m.get<i32>("port");
  REQUIRE(res.is_ok());
  CHECK(res.unwrap() == 9090);
}

TEST_CASE("Matches get<i32> on non-numeric text is InvalidArgument",
          "[arg][matches]") {
  Matches m;
  m.add("port", "not-a-number");

  auto res = m.get<i32>("port");
  REQUIRE(res.is_err());
  CHECK(res.unwrap_err() == Matches::GetError::InvalidArgument);
}

TEST_CASE("Matches get<double> parses floating point values",
          "[arg][matches]") {
  Matches m;
  m.add("ratio", "3.14");

  auto res = m.get<double>("ratio");
  REQUIRE(res.is_ok());
  CHECK(res.unwrap() == 3.14);
}

TEST_CASE("Matches get<bool> recognizes common truthy/falsy tokens",
          "[arg][matches]") {
  Matches m;
  m.add("a", "true");
  m.add("b", "1");
  m.add("c", "y");
  m.add("d", "false");
  m.add("e", "0");
  m.add("f", "n");

  CHECK(m.get<bool>("a").unwrap() == true);
  CHECK(m.get<bool>("b").unwrap() == true);
  CHECK(m.get<bool>("c").unwrap() == true);
  CHECK_FALSE(m.get<bool>("d").unwrap());
  CHECK_FALSE(m.get<bool>("e").unwrap());
  CHECK_FALSE(m.get<bool>("f").unwrap());
}

TEST_CASE("Matches get<bool> rejects unrecognized tokens", "[arg][matches]") {
  Matches m;
  m.add("verbose", "maybe");

  auto res = m.get<bool>("verbose");
  REQUIRE(res.is_err());
  CHECK(res.unwrap_err() == Matches::GetError::InvalidArgument);
}

TEST_CASE("Matches supports multiple distinct names", "[arg][matches]") {
  Matches m;
  m.add("port", "8080");
  m.add("host", "localhost");

  CHECK(m.get<std::string_view>("port").unwrap() == "8080");
  CHECK(m.get<std::string_view>("host").unwrap() == "localhost");
}

TEST_CASE("Matches get<T> on duplicate names returns the first match",
          "[arg][matches]") {
  // Current first-match-wins behavior; if Command::parse ever
  // allows repeated options to overwrite instead of accumulate, this test
  // should be revisited alongside that change.
  Matches m;
  m.add("tag", "first");
  m.add("tag", "second");

  auto res = m.get<std::string_view>("tag");
  REQUIRE(res.is_ok());
  CHECK(res.unwrap() == "first");
}

TEST_CASE("Matches positionals collect in insertion order", "[arg][matches]") {
  Matches m;
  CHECK(m.positionals().empty());

  m.add_positional("first");
  m.add_positional("second");

  REQUIRE(m.positionals().size() == 2);
  CHECK(m.positionals()[0] == "first");
  CHECK(m.positionals()[1] == "second");
}

}  // namespace arg
