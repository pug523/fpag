// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/concurrent_swiss_table.h"

#include <atomic>
#include <barrier>
#include <thread>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"

namespace base {

namespace {

constexpr usize kSmallCapacity = 16;
constexpr usize kMediumCapacity = 1024;
constexpr usize kLargeCapacity = 65536;

// Custom Hash functor to force hash collisions for probing validation.
struct CollisionHash {
  usize operator()(u64 key) const noexcept {
    // Map all keys to 0 or same bucket boundary to force linear/quadratic
    // probe collision.
    return static_cast<usize>(key % 4);
  }
};

}  // namespace

TEST_CASE("ConcurrentSwissTable basic single-threaded operations",
          "[base][concurrent_swiss_table]") {
  SECTION("Insert and find simple key-value pairs") {
    ConcurrentSwissTable<u64, u64> table(kSmallCapacity);

    u64 out_value = 0;
    CHECK_FALSE(table.find(42, &out_value));

    REQUIRE(table.insert(42, 100));
    CHECK(table.find(42, &out_value));
    CHECK(out_value == 100);

    REQUIRE(table.insert(100, 200));
    CHECK(table.find(100, &out_value));
    CHECK(out_value == 200);
  }

  SECTION("Duplicate insert fails and retains original value") {
    ConcurrentSwissTable<u64, u64> table(kSmallCapacity);

    REQUIRE(table.insert(1, 100));

    CHECK_FALSE(table.insert(1, 999));

    u64 out_value = 0;
    CHECK(table.find(1, &out_value));
    CHECK(out_value == 100);
  }

  SECTION("Find with nullptr out_value does not crash") {
    ConcurrentSwissTable<u64, u64> table(kSmallCapacity);

    REQUIRE(table.insert(10, 500));
    CHECK(table.find(10, nullptr));
    CHECK_FALSE(table.find(99, nullptr));
  }
}

TEST_CASE("ConcurrentSwissTable collision handling",
          "[base][concurrent_swiss_table]") {
  SECTION("Multiple elements with hash collisions can all be found") {
    ConcurrentSwissTable<u64, u64, CollisionHash> table(kSmallCapacity);

    // All these keys will result in identical hash probes.
    constexpr usize kNumItems = 8;
    for (u64 i = 1; i <= kNumItems; ++i) {
      REQUIRE(table.insert(i * 4, i * 100));
    }

    for (u64 i = 1; i <= kNumItems; ++i) {
      u64 out_value = 0;
      CHECK(table.find(i * 4, &out_value));
      CHECK(out_value == i * 100);
    }
  }
}

TEST_CASE("ConcurrentSwissTable capacity limit boundary",
          "[base][concurrent_swiss_table]") {
  SECTION("Table graceful rejection when capacity is full") {
    ConcurrentSwissTable<u64, u64> table(kSmallCapacity);

    usize inserted_count = 0;
    for (usize i = 0; i < kSmallCapacity * 2; ++i) {
      if (table.insert(static_cast<u64>(i), static_cast<u64>(i))) {
        ++inserted_count;
      }
    }

    // Table must not allow inserting more than its maximum available capacity.
    CHECK(inserted_count <= kSmallCapacity);

    // Read back all successfully inserted items.
    usize read_count = 0;
    for (usize i = 0; i < kSmallCapacity * 2; ++i) {
      u64 out_val = 0;
      if (table.find(static_cast<u64>(i), &out_val)) {
        CHECK(out_val == static_cast<u64>(i));
        ++read_count;
      }
    }
    CHECK(read_count == inserted_count);
  }
}

TEST_CASE("ConcurrentSwissTable multithreaded deduplication stress test",
          "[base][concurrent_swiss_table]") {
  SECTION("Exactly one thread succeeds in inserting duplicate key") {
    ConcurrentSwissTable<u64, u64> table(kMediumCapacity);

    constexpr usize kNumThreads = 16;
    constexpr u64 kTargetKey = 0xDEADBEEF;

    std::atomic<usize> success_count{0};
    std::atomic<usize> fail_count{0};
    std::barrier sync_point(static_cast<i64>(kNumThreads));

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (usize i = 0; i < kNumThreads; ++i) {
      threads.emplace_back([&, thread_id = i]() {
        // Synchronize all threads to start insertion simultaneously to trigger
        // race condition.
        sync_point.arrive_and_wait();

        const bool inserted =
            table.insert(kTargetKey, static_cast<u64>(thread_id + 1));
        if (inserted) {
          success_count.fetch_add(1, std::memory_order_relaxed);
        } else {
          fail_count.fetch_add(1, std::memory_order_relaxed);
        }
      });
    }

    for (auto& thread : threads) {
      thread.join();
    }

    CHECK(success_count.load() == 1);
    CHECK(fail_count.load() == kNumThreads - 1);

    u64 stored_value = 0;
    CHECK(table.find(kTargetKey, &stored_value));
    CHECK(stored_value >= 1);
    CHECK(stored_value <= kNumThreads);
  }
}

TEST_CASE("ConcurrentSwissTable concurrent disjoint insert and search",
          "[base][concurrent_swiss_table]") {
  SECTION("High concurrency distinct keys insertion and read validation") {
    ConcurrentSwissTable<u64, u64> table(kLargeCapacity);

    constexpr usize kNumWriterThreads = 8;
    constexpr usize kItemsPerThread = 2000;
    constexpr usize kTotalItems = kNumWriterThreads * kItemsPerThread;

    std::barrier sync_point(static_cast<i64>(kNumWriterThreads));
    std::vector<std::thread> writers;
    writers.reserve(kNumWriterThreads);

    for (usize thread_idx = 0; thread_idx < kNumWriterThreads; ++thread_idx) {
      writers.emplace_back([&, thread_idx]() {
        const u64 start_key = thread_idx * kItemsPerThread;
        sync_point.arrive_and_wait();

        for (usize i = 0; i < kItemsPerThread; ++i) {
          const u64 key = start_key + i;
          const u64 value = key ^ 0x55555555CAFEF00D;
          REQUIRE(table.insert(key, value));
        }
      });
    }

    for (auto& thread : writers) {
      thread.join();
    }

    // Verify all written values concurrently.
    constexpr usize kNumReaderThreads = 8;
    std::vector<std::thread> readers;
    readers.reserve(kNumReaderThreads);

    std::atomic<bool> all_matched{true};

    for (usize thread_idx = 0; thread_idx < kNumReaderThreads; ++thread_idx) {
      readers.emplace_back([&, thread_idx]() {
        const usize chunk = kTotalItems / kNumReaderThreads;
        const usize start = thread_idx * chunk;
        const usize end =
            (thread_idx == kNumReaderThreads - 1) ? kTotalItems : start + chunk;

        for (usize i = start; i < end; ++i) {
          const u64 key = static_cast<u64>(i);
          const u64 expected_value = key ^ 0x55555555CAFEF00D;
          u64 out_value = 0;

          if (!table.find(key, &out_value) || out_value != expected_value) {
            all_matched.store(false, std::memory_order_relaxed);
          }
        }
      });
    }

    for (auto& thread : readers) {
      thread.join();
    }

    CHECK(all_matched.load());
  }
}

TEST_CASE("ConcurrentSwissTable race condition reader-writer integrity test",
          "[base][concurrent_swiss_table]") {
  SECTION("Readers should never observe partial/corrupted writes") {
    ConcurrentSwissTable<u64, u64> table(kLargeCapacity);

    constexpr usize kNumOps = 20000;
    std::atomic<bool> stop_flag{false};

    // Writer thread continuously inserts new keys.
    std::thread writer([&]() {
      for (u64 i = 1; i <= kNumOps; ++i) {
        // Value is guaranteed to be a deterministic function of key.
        const u64 val = i * 31;
        table.insert(i, val);
      }
      stop_flag.store(true, std::memory_order_release);
    });

    // Reader thread continuously searches for keys being concurrently inserted.
    std::thread reader([&]() {
      while (!stop_flag.load(std::memory_order_acquire)) {
        for (u64 i = 1; i <= kNumOps; ++i) {
          u64 out_val = 0;
          if (table.find(i, &out_val)) {
            // Memory order check: If find() returns true, out_val MUST be
            // completely written.
            CHECK(out_val == i * 31);
          }
        }
      }
    });

    writer.join();
    reader.join();
  }
}

}  // namespace base

