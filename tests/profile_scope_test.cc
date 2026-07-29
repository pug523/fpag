// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/profiler/profile_scope.h"

#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fpag/debug/profiler/profiler.h"

namespace base {

namespace {

void dummy_profiled_function(Profiler* profiler) {
  PROFILE_FUNCTION_WITH_PROFILER(profiler);
}

}  // namespace

TEST_CASE("ProfileScope RAII measurement", "[base][profiler][scope]") {
  Profiler test_profiler;
  test_profiler.start();

  SECTION("Explicit PROFILE_SCOPE records start and duration") {
    {
      PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(&test_profiler, "custom_scope",
                                               "compiler");
    }

    auto events = test_profiler.events();
    REQUIRE(events.size() == 1);
    CHECK(std::string_view(events[0].name) == "custom_scope");
    CHECK(std::string_view(events[0].category) == "compiler");
    CHECK(events[0].duration_ns >= 0);
  }

  SECTION("PROFILE_FUNCTION records pretty function name") {
    dummy_profiled_function(&test_profiler);

    auto events = test_profiler.events();
    REQUIRE(events.size() == 1);
    CHECK(std::string_view(events[0].category) == "default");
    CHECK_FALSE(std::string_view(events[0].name).empty());
  }

  test_profiler.stop();
}

}  // namespace base
