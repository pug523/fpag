// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/container/simple_concurrent_hash_map.h"

#include <atomic>
#include <thread>
#include <vector>

#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"

namespace container {

TEST_CASE("SimpleConcurrentHashMap basic operations",
          "[base][container][hashmap]") {
  SimpleConcurrentHashMap<u64, u64> map(1024);

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

namespace {

// A value large enough that copying it into a claimed slot takes long enough
// for another thread that saw the slot empty and then lost the claim to still
// find it claimed when it looks again, and for two writers of the same key to
// interleave their copies.
struct BigValue {
  u8 bytes[4096];
};

}  // namespace

TEST_CASE("SimpleConcurrentHashMap keeps one entry per key under contention",
          "[base][container][hashmap]") {
  constexpr u32 kThreads = 8;
  constexpr u32 kKeys = 8;
  constexpr u32 kRounds = 32;

  SimpleConcurrentHashMap<u64, BigValue> map(1024);

  // Every thread inserts every key, over and over, so the same slot is claimed
  // by all of them at once. A lost claim has to look at that slot again: if it
  // probes on instead, it finds an empty slot and stores a second entry for a
  // key that is already in the map. An update that does not own the slot races
  // with the other writers, and the 4 KiB copy tears.
  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (u32 t = 0; t < kThreads; ++t) {
    threads.emplace_back([&map, t] {
      BigValue value{};
      for (u8& byte : value.bytes) {
        byte = static_cast<u8>(t);
      }
      for (u32 round = 0; round < kRounds; ++round) {
        for (u32 k = 0; k < kKeys; ++k) {
          map.insert(k, value);
        }
      }
    });
  }

  for (std::thread& t : threads) {
    t.join();
  }

  CHECK(map.size() == kKeys);
  for (u32 k = 0; k < kKeys; ++k) {
    const BigValue* v = map.find(k);
    REQUIRE(v != nullptr);
    CHECK(v->bytes[0] < kThreads);
    for (usize i = 1; i < sizeof(v->bytes); ++i) {
      if (v->bytes[i] != v->bytes[0]) {
        CHECK(v->bytes[i] == v->bytes[0]);
        break;
      }
    }
  }
}

TEST_CASE("SimpleConcurrentHashMap try_insert has one winner per key",
          "[base][container][hashmap]") {
  constexpr u32 kThreads = 8;
  constexpr u32 kKeys = 32;
  constexpr u32 kRounds = 64;

  SimpleConcurrentHashMap<u64, u64> map(1024);

  std::vector<std::atomic<u32>> insertions(kKeys);
  for (std::atomic<u32>& count : insertions) {
    count.store(0, std::memory_order_relaxed);
  }

  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (u32 t = 0; t < kThreads; ++t) {
    threads.emplace_back([&map, &insertions, t] {
      for (u32 round = 0; round < kRounds; ++round) {
        for (u32 k = 0; k < kKeys; ++k) {
          bool inserted = false;
          map.try_insert(k, t, &inserted);
          if (inserted) {
            insertions[k].fetch_add(1, std::memory_order_relaxed);
          }
        }
      }
    });
  }

  for (std::thread& t : threads) {
    t.join();
  }

  CHECK(map.size() == kKeys);
  for (u32 k = 0; k < kKeys; ++k) {
    CHECK(insertions[k].load(std::memory_order_relaxed) == 1);
  }
}

TEST_CASE("SimpleConcurrentHashMap reset releases the entries",
          "[base][container][hashmap]") {
  SimpleConcurrentHashMap<u64, u64> map(1024);
  map.insert(7, 70);
  REQUIRE(map.find(7) != nullptr);

  map.reset();

  // Nothing is left to read, and a second reset, or the destructor's own, has
  // nothing to release.
  CHECK(map.capacity() == 0);
  CHECK(map.size() == 0);
  CHECK(map.find(7) == nullptr);
  map.reset();

  // The map stays usable, and a later reserve() discards what was in it.
  map.reserve(1024);
  map.insert(7, 70);
  CHECK(map.size() == 1);

  map.reserve(8192);
  CHECK(map.capacity() == 8192);
  CHECK(map.size() == 0);
  CHECK(map.find(7) == nullptr);

  map.insert(9, 90);
  const u64* v = map.find(9);
  REQUIRE(v != nullptr);
  CHECK(*v == 90);
}

TEST_CASE("SimpleConcurrentHashMap thread-safety stress test",
          "[base][container][stress]") {
  const u64 capacity = 1 << 16;
  SimpleConcurrentHashMap<u64, u64> map(capacity);

  const u32 num_threads = std::thread::hardware_concurrency();
  const u32 inserts_per_thread = 1000;

  SECTION("Concurrent inserts of unique keys") {
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (u32 i = 0; i < num_threads; ++i) {
      threads.emplace_back([&map, i]() {
        for (u32 j = 0; j < inserts_per_thread; ++j) {
          const u64 key = i * inserts_per_thread + j;
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
    // One value should be stored (0 ~ num_threads-1)
    CHECK(*v < num_threads);
  }
}

}  // namespace container
