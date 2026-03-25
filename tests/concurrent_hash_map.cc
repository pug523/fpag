// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/concurrent_hash_map.h"

#include <thread>
#include <vector>

#include "base/numeric.h"
#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"

namespace base {

TEST_CASE("ConcurrentHashMap basic operations", "[base][container][hashmap]") {
  ConcurrentHashMap<u64, u64> map(1024);

  SECTION("Insert and find") {
    map.insert(42, 100);
    map.insert(123, 456);

    const u64* v1 = map.find(42);
    const u64* v2 = map.find(123);
    const u64* v3 = map.find(999);  // does not exist

    REQUIRE(v1 != nullptr);
    CHECK(*v1 == 100);
    REQUIRE(v2 != nullptr);
    CHECK(*v2 == 456);
    CHECK(v3 == nullptr);
  }

  SECTION("Update existing key") {
    map.insert(10, 100);
    // existing key, so value is updated
    map.insert(10, 200);

    const u64* v = map.find(10);
    REQUIRE(v != nullptr);
    CHECK(*v == 200);
  }
}

// TODO: add `try_insert` tests

TEST_CASE("ConcurrentHashMap thread-safety stress test",
          "[base][container][stress]") {
  const u64 capacity = 1 << 16;
  ConcurrentHashMap<u64, u64> map(capacity);

  const u32 num_threads = std::thread::hardware_concurrency();
  const u32 inserts_per_thread = 1000;

  SECTION("Concurrent inserts of unique keys") {
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (u32 i = 0; i < num_threads; ++i) {
      threads.emplace_back([&map, i]() {
        for (u32 j = 0; j < inserts_per_thread; ++j) {
          u64 key = i * inserts_per_thread + j;
          map.insert(key, key * 10);
        }
      });
    }

    for (std::thread& t : threads) {
      t.join();
    }

    for (u32 i = 0; i < num_threads * inserts_per_thread; ++i) {
      const u64* v = map.find(i);
      CAPTURE(i);
      REQUIRE(v != nullptr);
      CHECK(*v == (u64)i * 10);
    }
  }

  SECTION("Concurrent inserts of same keys") {
    // All threads insert the same key (1), so only one value should be stored
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (u32 i = 0; i < num_threads; ++i) {
      threads.emplace_back([&map, i]() { map.insert(1, i); });
    }

    for (std::thread& t : threads) {
      t.join();
    }

    const u64* v = map.find(1);
    REQUIRE(v != nullptr);
    // One value should be stored (0 〜 num_threads-1)
    CHECK(*v < num_threads);
  }
}

}  // namespace base
