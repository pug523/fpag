// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/location.h"

#include <string_view>

#include "catch2/catch_test_macros.hpp"

namespace base {

TEST_CASE("Location default initialization and validity", "[base][location]") {
  SECTION("Default constructed Location") {
    Location const loc{};
    CHECK(loc.file_name().empty());
    CHECK(loc.function_name().empty());
    CHECK(loc.line == 0);
    CHECK(loc.column == 0);

    CHECK_FALSE(loc.valid_file());
    CHECK_FALSE(loc.valid_function());
    CHECK_FALSE(loc.valid_line());
    CHECK_FALSE(loc.valid_column());
  }

  SECTION("Custom populated Location") {
    Location const loc{
        .file = "test_file.cc",
        .function = "test_func",
        .line = 168,
        .column = 8000,
    };

    CHECK(loc.file_name() == "test_file.cc");
    CHECK(loc.function_name() == "test_func");
    CHECK(loc.valid_file());
    CHECK(loc.valid_function());
    CHECK(loc.valid_line());
    CHECK(loc.valid_column());
  }
}

TEST_CASE("FROM_HERE macro validation", "[base][location]") {
  const Location loc = FROM_HERE();

  CHECK(loc.valid_file());
  CHECK(loc.valid_function());
  CHECK(loc.valid_line());
  CHECK(loc.file_name().ends_with("location_test.cc"));
  CHECK_FALSE(loc.function_name().empty());
  CHECK(loc.line > 0);
}

}  // namespace base
