// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/converter.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "catch2/catch_test_macros.hpp"

namespace arg {

// NOLINTBEGIN(bugprone-use-after-move)

TEST_CASE("Parsable concept check", "[arg][converter]") {
  STATIC_CHECK(Parsable<std::string_view>);
  STATIC_CHECK(Parsable<std::string>);
  STATIC_CHECK(Parsable<bool>);
  STATIC_CHECK(Parsable<int>);
  STATIC_CHECK(Parsable<int64_t>);
  STATIC_CHECK(Parsable<uint32_t>);
  STATIC_CHECK(Parsable<float>);
  STATIC_CHECK(Parsable<double>);

  struct CustomUnparsedType {};
  STATIC_CHECK_FALSE(Parsable<CustomUnparsedType>);
}

TEST_CASE("Converter<std::string_view>", "[arg][converter]") {
  auto res = Converter<std::string_view>::from_string("hello");
  REQUIRE(res.is_ok());
  CHECK(std::move(res).unwrap() == "hello");
}

TEST_CASE("Converter<std::string>", "[arg][converter]") {
  auto res = Converter<std::string>::from_string("world");
  REQUIRE(res.is_ok());
  CHECK(std::move(res).unwrap() == "world");
}

TEST_CASE("Converter<bool> parses truthy values", "[arg][converter]") {
  for (const std::string_view val : {"true", "1", "y"}) {
    auto res = Converter<bool>::from_string(val);
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap() == true);
  }
}

TEST_CASE("Converter<bool> parses falsy values", "[arg][converter]") {
  for (const std::string_view val : {"false", "0", "n"}) {
    auto res = Converter<bool>::from_string(val);
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap() == false);
  }
}

TEST_CASE("Converter<bool> rejects invalid string inputs", "[arg][converter]") {
  for (const std::string_view val :
       {"TRUE", "False", "yes", "no", "2", "invalid"}) {
    auto res = Converter<bool>::from_string(val);
    REQUIRE(res.is_err());
    CHECK(std::move(res).unwrap_err() == GetError::InvalidArgument);
  }
}

TEST_CASE("Converter<int> parses valid integers", "[arg][converter]") {
  SECTION("Positive integer") {
    auto res = Converter<int>::from_string("12345");
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap() == 12345);
  }

  SECTION("Negative integer") {
    auto res = Converter<int>::from_string("-42");
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap() == -42);
  }

  SECTION("Zero") {
    auto res = Converter<int>::from_string("0");
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap() == 0);
  }
}

TEST_CASE("Converter<int> rejects invalid inputs", "[arg][converter]") {
  SECTION("Non-numeric characters") {
    auto res = Converter<int>::from_string("abc");
    REQUIRE(res.is_err());
    CHECK(std::move(res).unwrap_err() == GetError::InvalidArgument);
  }

  SECTION("Empty string") {
    auto res = Converter<int>::from_string("");
    REQUIRE(res.is_err());
    CHECK(std::move(res).unwrap_err() == GetError::InvalidArgument);
  }

  SECTION("Trailing characters") {
    // std::from_chars parses valid prefix; check if behavior matches
    // requirement
    auto res = Converter<int>::from_string("123abc");
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap() == 123);
  }

  SECTION("Overflow") {
    auto res = Converter<int>::from_string("999999999999999999999999");
    REQUIRE(res.is_err());
    CHECK(std::move(res).unwrap_err() == GetError::InvalidArgument);
  }
}

TEST_CASE("Converter<double> parses valid floating-point numbers",
          "[arg][converter]") {
  SECTION("Positive double") {
    auto res = Converter<double>::from_string("3.14159");
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap() == 3.14159);
  }

  SECTION("Negative double") {
    auto res = Converter<double>::from_string("-0.00123");
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap() == -0.00123);
  }
}

TEST_CASE("Converter<double> rejects invalid float inputs",
          "[arg][converter]") {
  auto res = Converter<double>::from_string("invalid_float");
  REQUIRE(res.is_err());
  CHECK(std::move(res).unwrap_err() == GetError::InvalidArgument);
}

// NOLINTEND(bugprone-use-after-move)

}  // namespace arg
