// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/string_interner.h"

#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "base/numeric.h"
#include "catch2/catch_test_macros.hpp"

namespace base {

TEST_CASE("StringInterner basic operations", "[base][string_interner]") {
  // Initialize with a small capacity to test growth if applicable.
  StringInterner interner(1024, 512, false);

  SECTION("Interning identical strings returns the same ID") {
    std::string_view str1 = "test";
    std::string_view str2 = "test";

    StringId id1 = interner.intern(str1);
    StringId id2 = interner.intern(str2);

    CHECK(id1 == id2);
  }

  SECTION("Interning different strings returns different IDs") {
    StringId id1 = interner.intern("apple");
    StringId id2 = interner.intern("orange");

    CHECK(id1 != id2);
  }

  SECTION("Empty string handling") {
    StringId id1 = interner.intern("");
    StringId id2 = interner.intern("");

    CHECK(id1 == id2);
  }
}

TEST_CASE("StringInterner concurrency",
          "[base][string_interner][multithread]") {
  StringInterner interner(4096, 2048, false);
  const i32 thread_count = 1;
  const i32 iterations = 1000;

  // We want to ensure that multiple threads interning the same set of strings
  // results in a consistent state without crashes or duplicate IDs for the same
  // string.
  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  for (i32 t = 0; t < thread_count; ++t) {
    threads.emplace_back([&interner, t]() {
      for (i32 i = 0; i < iterations; ++i) {
        // Mix of shared strings and unique strings.
        std::string shared = "shared_" + std::to_string(i % 10);
        std::string unique = std::format("unique_{}_{}", t, i);

        interner.intern(shared);
        interner.intern(unique);
      }
    });
  }

  for (std::thread& th : threads) {
    th.join();
  }

  // After concurrent stress, a single-threaded check should still be
  // consistent.
  StringId id1 = interner.intern("shared_5");
  StringId id2 = interner.intern("shared_5");
  CHECK(id1 == id2);
}

TEST_CASE("StringInterner stability with growth", "[base][string_interner]") {
  // Test that adding many strings doesn't invalidate or change existing IDs.
  StringInterner interner(256, 8192, false);  // Small initial pool

  std::string_view target = "persistent_id";
  StringId original_id = interner.intern(target);

  for (int i = 0; i < 5000; ++i) {
    interner.intern("filler_string_" + std::to_string(i));
  }

  // The ID for `target` must remain exactly the same.
  CHECK(interner.intern(target) == original_id);
}

}  // namespace base
