// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/result.h"

#include <string_view>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"

namespace base {

namespace {

enum class ErrorCode : u8 {
  NotFound,
  PermissionDenied,
  InternalError,
};

}  // namespace

TEST_CASE("Result construction and status checks", "[base][result]") {
  SECTION("Ok variant construction via make_ok") {
    auto res = Result<i32, ErrorCode>(make_ok(100));
    REQUIRE(res.is_ok());
    CHECK_FALSE(res.is_err());
    CHECK(res.value() == 100);

    auto void_res = Result<void, ErrorCode>(make_ok());
    REQUIRE(void_res.is_ok());
    CHECK_FALSE(void_res.is_err());
  }

  SECTION("Err variant construction via make_err") {
    auto res = Result<i32, ErrorCode>(make_err(ErrorCode::NotFound));
    REQUIRE(res.is_err());
    CHECK_FALSE(res.is_ok());

    auto void_res = Result<void, ErrorCode>(make_err(ErrorCode::NotFound));
    REQUIRE(void_res.is_err());
    CHECK_FALSE(void_res.is_ok());
  }
}

TEST_CASE("Result unwrap operations", "[base][result]") {
  SECTION("unwrap on Ok") {
    auto res = Result<std::string_view, ErrorCode>(
        make_ok(std::string_view{"success"}));
    CHECK(std::move(res).unwrap() == "success");
  }

  SECTION("unwrap_err on Err") {
    auto res = Result<i32, ErrorCode>(make_err(ErrorCode::PermissionDenied));
    CHECK(std::move(res).unwrap_err() == ErrorCode::PermissionDenied);

    auto void_res =
        Result<void, ErrorCode>(make_err(ErrorCode::PermissionDenied));
    CHECK(std::move(void_res).unwrap_err() == ErrorCode::PermissionDenied);
  }

  SECTION("unwrap_or with fallback value") {
    SECTION("returns contained value when Ok") {
      auto res = Result<i32, ErrorCode>(make_ok(42));
      CHECK(std::move(res).unwrap_or(0) == 42);
    }

    SECTION("returns default value when Err") {
      auto res = Result<i32, ErrorCode>(make_err(ErrorCode::InternalError));
      CHECK(std::move(res).unwrap_or(0) == 0);
    }
  }
}

TEST_CASE("Result value inspection", "[base][result]") {
  auto res = Result<i32, ErrorCode>(make_ok(50));

  SECTION("Const value reference") {
    const auto& const_res = res;
    CHECK(const_res.value() == 50);
  }

  SECTION("Mutable value reference") {
    res.value() += 10;
    CHECK(res.value() == 60);
  }
}

TEST_CASE("Result map combinator", "[base][result]") {
  SECTION("map transforms Ok value") {
    auto res = Result<i32, ErrorCode>(make_ok(10));
    auto mapped = std::move(res).map([](i32 val) { return val * 2; });

    REQUIRE(mapped.is_ok());
    CHECK(std::move(mapped).unwrap() == 20);
  }

  SECTION("map preserves Err value") {
    auto res = Result<i32, ErrorCode>(make_err(ErrorCode::NotFound));
    auto mapped = std::move(res).map([](i32 val) { return val * 2; });

    REQUIRE(mapped.is_err());
    CHECK(std::move(mapped).unwrap_err() == ErrorCode::NotFound);

    auto void_res = Result<void, ErrorCode>(make_err(ErrorCode::NotFound));
    auto void_mapped = std::move(void_res).map([]() {});

    REQUIRE(void_mapped.is_err());
    CHECK(std::move(void_mapped).unwrap_err() == ErrorCode::NotFound);
  }
}

TEST_CASE("Result and_then combinator", "[base][result]") {
  auto double_if_positive = [](i32 val) -> Result<i32, ErrorCode> {
    if (val > 0) {
      return make_ok(val * 2);
    }
    return make_err(ErrorCode::InternalError);
  };

  SECTION("and_then chains operations on Ok") {
    auto res = Result<i32, ErrorCode>(make_ok(5));
    auto chained = std::move(res).and_then(double_if_positive);

    REQUIRE(chained.is_ok());
    CHECK(std::move(chained).unwrap() == 10);
  }

  SECTION("and_then short-circuits on inner Err") {
    auto res = Result<i32, ErrorCode>(make_ok(-10));
    auto chained = std::move(res).and_then(double_if_positive);

    REQUIRE(chained.is_err());
    CHECK(std::move(chained).unwrap_err() == ErrorCode::InternalError);
  }

  SECTION("and_then short-circuits on initial Err") {
    auto res = Result<i32, ErrorCode>(make_err(ErrorCode::NotFound));
    auto chained = std::move(res).and_then(double_if_positive);

    REQUIRE(chained.is_err());
    CHECK(std::move(chained).unwrap_err() == ErrorCode::NotFound);
  }
}

}  // namespace base
