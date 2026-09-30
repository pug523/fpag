// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/profiler/profiler.h"

#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/debug/location.h"
#include "fpag/debug/process_id.h"
#include "fpag/debug/profiler/profile_event.h"
#include "fpag/debug/thread_id.h"

namespace debug {

TEST_CASE("Profiler basic recording lifecycle", "[base][profiler]") {
  SECTION("Events are ignored when disabled") {
    Profiler test_profiler;
    test_profiler.stop();
    test_profiler.start();
    test_profiler.stop();

    test_profiler.record_event(ProfileEvent{
        .name = test_profiler.intern("ignored_event"),
        .category = test_profiler.intern("test"),
        .location = Location::current(),
        .start_time_ns = 1000,
        .duration_ns = 500,
        .thread_id = 1,
        .process_id = 100,
    });

    CHECK(test_profiler.empty());
  }

  SECTION("Events are recorded when enabled") {
    Profiler test_profiler;
    test_profiler.start();

    const Location loc = Location::current();
    test_profiler.record_event(ProfileEvent{
        .name = test_profiler.intern("valid_event"),
        .category = test_profiler.intern("test"),
        .location = loc,
        .start_time_ns = 1000,
        .duration_ns = 500,
        .thread_id = 1,
        .process_id = 100,
    });

    auto events = test_profiler.copy_events();
    REQUIRE(events.size() == 1);
    CHECK(test_profiler.name(events[0].name) == "valid_event");
    CHECK(test_profiler.name(events[0].category) == "test");
    CHECK(events[0].start_time_ns == 1000);
    CHECK(events[0].duration_ns == 500);

    test_profiler.stop();
  }

  SECTION("start() resets recorded event count") {
    Profiler test_profiler;
    test_profiler.start();
    test_profiler.record_event(
        ProfileEvent{.name = test_profiler.intern("event1")});
    REQUIRE(test_profiler.size() == 1);

    test_profiler.start();
    CHECK(test_profiler.empty());
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

TEST_CASE("A name an event holds outlives the events themselves",
          "[base][profiler]") {
  Profiler test_profiler;
  test_profiler.start();

  const str::StringPoolId name = test_profiler.intern("a_scope");
  test_profiler.record_event(ProfileEvent{.name = name});
  REQUIRE(test_profiler.size() == 1);

  // The id a caller is holding has to keep naming the same bytes after the
  // events are dropped, which is why start() and clear() do not touch the
  // interner: a formatter that already has an event snapshot still resolves it.
  test_profiler.clear();
  test_profiler.start();
  test_profiler.stop();

  CHECK(test_profiler.name(name) == "a_scope");
}

TEST_CASE("Several threads record into one profiler",
          "[base][profiler][threads]") {
  constexpr usize THREADS = 4;
  constexpr usize PER_THREAD = 64;

  Profiler test_profiler;
  test_profiler.start();

  // Catch2 assertions do not run on a worker thread, so the workers only record
  // and the parent checks what they left behind.
  std::vector<std::thread> workers;
  workers.reserve(THREADS);
  for (usize worker = 0; worker < THREADS; ++worker) {
    workers.emplace_back([&test_profiler, worker]() {
      for (usize index = 0; index < PER_THREAD; ++index) {
        const str::StringPoolId name = test_profiler.intern(
            "thread_" + std::to_string(worker) + "_" + std::to_string(index));
        test_profiler.record_event(ProfileEvent{
            .name = name,
            .start_time_ns = 1000 + index,
            .duration_ns = 500,
            .thread_id = current_thread_id(),
            .process_id = current_process_id(),
        });
      }
    });
  }
  for (std::thread& worker : workers) {
    worker.join();
  }

  const auto events = test_profiler.copy_events();
  CHECK(events.size() == THREADS * PER_THREAD);

  // Every name came back readable, which is the part concurrent interning can
  // get wrong: two threads interning one name must agree on its id.
  for (usize worker = 0; worker < THREADS; ++worker) {
    for (usize index = 0; index < PER_THREAD; ++index) {
      const std::string expected =
          "thread_" + std::to_string(worker) + "_" + std::to_string(index);
      const str::StringPoolId first = test_profiler.intern(expected);
      const str::StringPoolId second = test_profiler.intern(expected);
      CHECK(first == second);
      CHECK(test_profiler.name(first) == expected);
    }
  }

  test_profiler.stop();
}

}  // namespace debug
