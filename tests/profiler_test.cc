// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/profiler/profiler.h"

#include <span>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/debug/process_id.h"
#include "fpag/base/debug/thread_id.h"
#include "fpag/base/location.h"
#include "fpag/base/profiler/profile_event.h"

namespace base {

TEST_CASE("Profiler basic recording lifecycle", "[base][profiler]") {
  SECTION("Events are ignored when disabled") {
    Profiler test_profiler;
    test_profiler.stop();
    test_profiler.start();
    test_profiler.stop();

    test_profiler.record_event(ProfileEvent{
        .name = "ignored_event",
        .category = "test",
        .location = Location::current(),
        .start_time_ns = 1000,
        .duration_ns = 500,
        .thread_id = 1,
        .process_id = 100,
    });

    CHECK(test_profiler.events().empty());
  }

  SECTION("Events are recorded when enabled") {
    Profiler test_profiler;
    test_profiler.start();

    const Location loc = Location::current();
    test_profiler.record_event(ProfileEvent{
        .name = "valid_event",
        .category = "test",
        .location = loc,
        .start_time_ns = 1000,
        .duration_ns = 500,
        .thread_id = 1,
        .process_id = 100,
    });

    auto events = test_profiler.events();
    REQUIRE(events.size() == 1);
    CHECK(std::string_view(events[0].name) == "valid_event");
    CHECK(std::string_view(events[0].category) == "test");
    CHECK(events[0].start_time_ns == 1000);
    CHECK(events[0].duration_ns == 500);

    test_profiler.stop();
  }

  SECTION("start() resets recorded event count") {
    Profiler test_profiler;
    test_profiler.start();
    test_profiler.record_event(ProfileEvent{.name = "event1"});
    REQUIRE(test_profiler.events().size() == 1);

    test_profiler.start();
    CHECK(test_profiler.events().empty());
    test_profiler.stop();
  }
}

TEST_CASE("Profiler capacity boundary checks", "[base][profiler]") {
  Profiler test_profiler;
  test_profiler.start();

  SECTION("Process thread ID helpers") {
    CHECK(current_thread_id() != 0);
    CHECK(current_process_id() != 0);
  }

  test_profiler.stop();
}

}  // namespace base
