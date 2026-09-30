// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/profiler/profile_section.h"

#include <string>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fpag/debug/profiler/profiler.h"

namespace debug {

TEST_CASE("ProfileSection manual and destructor-based measurement",
          "[base][profiler][section]") {
  Profiler test_profiler;
  test_profiler.start();

  SECTION(
      "Explicit PROFILE_SECTION_START and PROFILE_SECTION_END records event") {
    PROFILE_SECTION_START_WITH_CATEGORY_AND_PROFILER(
        sec, &test_profiler, "manual_section", "compiler");
    const int result = 168;
    PROFILE_SECTION_END(sec);

    CHECK(result == 168);

    auto events = test_profiler.copy_events();
    REQUIRE(events.size() == 1);
    CHECK(test_profiler.name(events[0].name) == "manual_section");
    CHECK(test_profiler.name(events[0].category) == "compiler");
  }

  SECTION(
      "ProfileSection doesn't record event if PROFILE_SECTION_END is omitted") {
    {
      PROFILE_SECTION_START_WITH_PROFILER(sec, &test_profiler,
                                          "auto_stop_section");
    }

    REQUIRE(test_profiler.size() == 0);
  }

  SECTION(
      "Calling PROFILE_SECTION_END multiple times is safe and records only "
      "once") {
    PROFILE_SECTION_START_WITH_PROFILER(sec, &test_profiler,
                                        "single_record_section");

    PROFILE_SECTION_END(sec);
    PROFILE_SECTION_END(sec);

    REQUIRE(test_profiler.size() == 1);
  }

  SECTION("A temporary name is still readable when the section ends") {
    // The temporary is destroyed at the end of the declaration statement, while
    // the event is recorded when the block closes, so a section holding the
    // caller's pointer reads freed memory here. Read back under ASan.
    PROFILE_SECTION_START_WITH_PROFILER(sec, &test_profiler,
                                        std::string("a_temporary_name"));
    const int work = 168;
    PROFILE_SECTION_END(sec);

    CHECK(work == 168);

    auto events = test_profiler.copy_events();
    REQUIRE(events.size() == 1);
    CHECK(test_profiler.name(events[0].name) == "a_temporary_name");
  }

  test_profiler.stop();
}

}  // namespace debug
