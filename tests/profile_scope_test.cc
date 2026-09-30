// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/profiler/profile_scope.h"

#include <string>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fpag/debug/profiler/profiler.h"

namespace debug {

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

    auto events = test_profiler.copy_events();
    REQUIRE(events.size() == 1);
    CHECK(test_profiler.name(events[0].name) == "custom_scope");
    CHECK(test_profiler.name(events[0].category) == "compiler");
  }

  SECTION("PROFILE_FUNCTION records pretty function name") {
    dummy_profiled_function(&test_profiler);

    auto events = test_profiler.copy_events();
    REQUIRE(events.size() == 1);
    CHECK(test_profiler.name(events[0].category) == "default");
    CHECK_FALSE(test_profiler.name(events[0].name).empty());
  }

  SECTION("A temporary name is still readable after the scope ends") {
    // The std::string is destroyed at the end of the statement that constructed
    // the scope, so the name has to be copied out of it there rather than at
    // stop(). Read back under ASan, which is where a name left pointing into
    // the freed temporary would be reported.
    { PROFILE_SCOPE_WITH_PROFILER(&test_profiler, std::string("temporary")); }
    {
      PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(
          &test_profiler, std::string("dynamic_") + std::to_string(42),
          std::string("category_from_a_temporary"));
    }

    auto events = test_profiler.copy_events();
    REQUIRE(events.size() == 2);
    CHECK(test_profiler.name(events[0].name) == "temporary");
    CHECK(test_profiler.name(events[1].name) == "dynamic_42");
    CHECK(test_profiler.name(events[1].category) ==
          "category_from_a_temporary");
  }

  SECTION(
      "A name and a category interned twice are one name and one category") {
    {
      PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(&test_profiler, "repeated",
                                               "shared");
    }
    {
      PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(&test_profiler, "repeated",
                                               "shared");
    }

    auto events = test_profiler.copy_events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].name == events[1].name);
    CHECK(events[0].category == events[1].category);
  }

  test_profiler.stop();
}

}  // namespace debug
