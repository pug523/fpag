// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/container/spsc_queue.h"

#include <atomic>
#include <thread>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"

namespace container {

namespace {

TEST_CASE("SpscQueue Constructor and Capacity", "[SpscQueueTest]") {
  SpscQueue queue_small;
  queue_small.init();
  CHECK(queue_small.capacity() == SpscQueue::default_capacity());
  CHECK(queue_small.empty());
  CHECK(queue_small.size() == 0);
  CHECK(queue_small.available() == SpscQueue::default_capacity());

  SpscQueue queue_large;
  queue_large.init(SpscQueue::default_capacity() * 1024);
  CHECK(queue_large.capacity() == SpscQueue::default_capacity() * 1024);
  CHECK(queue_large.empty());
  CHECK(queue_large.size() == 0);
  CHECK(queue_large.available() == SpscQueue::default_capacity() * 1024);
}

TEST_CASE("SpscQueue Enqueue Dequeue Single Element", "[SpscQueueTest]") {
  SpscQueue queue;
  queue.init();

  SECTION(
      "Manual enqueue(reserve/commit) and dequeue(peek/discard) single "
      "integer") {
    constexpr i32 kDataIn = 42;

    void* ptr = nullptr;
    const SpscQueue::EnqueueStatus result =
        queue.reserve(sizeof(kDataIn), &ptr);
    CHECK(result == SpscQueue::EnqueueStatus::Ok);
    *static_cast<i32*>(ptr) = kDataIn;
    queue.commit(sizeof(kDataIn));
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(kDataIn));
    CHECK(queue.available() == SpscQueue::default_capacity() - sizeof(kDataIn));

    const char* peeked = queue.peek(sizeof(kDataIn));
    const i32* data_out_ptr = reinterpret_cast<const i32*>(peeked);
    const i32 data_out_copied = *data_out_ptr;
    queue.discard(sizeof(data_out_copied));
    CHECK(queue.empty());
    CHECK(queue.size() == 0);
    CHECK(queue.available() == SpscQueue::default_capacity());
    CHECK(data_out_copied == kDataIn);
  }

  SECTION(
      "Manual enqueue(reserve/commit) and dequeue(peek/discard) single char") {
    constexpr char kCharIn = 'A';

    void* ptr = nullptr;
    const SpscQueue::EnqueueStatus result =
        queue.reserve(sizeof(kCharIn), &ptr);
    CHECK(result == SpscQueue::EnqueueStatus::Ok);
    *static_cast<char*>(ptr) = kCharIn;
    queue.commit(sizeof(kCharIn));
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(kCharIn));
    CHECK(queue.available() == SpscQueue::default_capacity() - sizeof(kCharIn));

    const char char_out =
        *reinterpret_cast<const char*>(queue.peek(sizeof(kCharIn)));
    queue.discard(sizeof(kCharIn));
    CHECK(queue.empty());
    CHECK(char_out == kCharIn);
  }

  SECTION("Automatic enqueue and dequeue single integer") {
    constexpr i32 kDataIn = 42;

    const SpscQueue::EnqueueStatus result =
        queue.enqueue(&kDataIn, sizeof(kDataIn));
    CHECK(result == SpscQueue::EnqueueStatus::Ok);
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(kDataIn));
    CHECK(queue.available() == SpscQueue::default_capacity() - sizeof(kDataIn));

    i32 data_out = 0;
    queue.dequeue(static_cast<void*>(&data_out), sizeof(data_out));
    CHECK(queue.empty());
    CHECK(queue.size() == 0);
    CHECK(queue.available() == SpscQueue::default_capacity());
    CHECK(data_out == kDataIn);
  }

  SECTION("Automatic enqueue and dequeue single char") {
    constexpr char kCharIn = 'A';

    const SpscQueue::EnqueueStatus result =
        queue.enqueue(&kCharIn, sizeof(kCharIn));
    CHECK(result == SpscQueue::EnqueueStatus::Ok);
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(kCharIn));
    CHECK(queue.available() == SpscQueue::default_capacity() - sizeof(kCharIn));

    char char_out = 0;
    queue.dequeue(&char_out, sizeof(kCharIn));
    CHECK(queue.empty());
    CHECK(char_out == kCharIn);
  }
}

TEST_CASE("SpscQueue hands a record from one thread to another",
          "[SpscQueueTest]") {
  // The consumer has to observe the record's payload, not only the counter:
  // size_consumer() pairs with the producer's release store, and that pairing
  // is what orders the consumer's reads after the producer's writes. With a
  // relaxed load the two race, which TSan reports for this case.
  constexpr usize kRecords = 4096;
  constexpr usize kRecordSize = 64;

  SpscQueue queue;
  queue.init(SpscQueue::default_capacity(), SpscQueue::Mode::Block);

  std::atomic<bool> mismatch{false};

  std::thread consumer([&queue, &mismatch] {
    std::vector<u8> out(kRecordSize);
    for (usize record = 0; record < kRecords; ++record) {
      SpscQueue::DequeueStatus status = SpscQueue::DequeueStatus::Empty;
      do {
        status = queue.dequeue(out.data(), out.size());
        if (status != SpscQueue::DequeueStatus::Ok) {
          std::this_thread::yield();
        }
      } while (status != SpscQueue::DequeueStatus::Ok);

      for (usize i = 0; i < kRecordSize; ++i) {
        if (out[i] != static_cast<u8>(record + i)) {
          mismatch.store(true);
        }
      }
    }
  });

  std::vector<u8> record(kRecordSize);
  for (usize r = 0; r < kRecords; ++r) {
    for (usize i = 0; i < kRecordSize; ++i) {
      record[i] = static_cast<u8>(r + i);
    }

    const SpscQueue::EnqueueStatus status =
        queue.enqueue(record.data(), record.size());
    CHECK(status == SpscQueue::EnqueueStatus::Ok);
  }

  consumer.join();
  CHECK_FALSE(mismatch.load());
}

TEST_CASE("SpscQueue refuses a record whose alignment padding does not fit",
          "[SpscQueueTest]") {
  SpscQueue queue;
  queue.init(4096);

  // Leave seven bytes free, then ask for a seven byte record aligned to eight.
  // The payload fits, the seven bytes of padding in front of it do not.
  std::vector<u8> fill(4089);
  for (usize i = 0; i < fill.size(); ++i) {
    fill[i] = static_cast<u8>(i % 251);
  }
  REQUIRE(queue.enqueue(fill.data(), fill.size()) ==
          SpscQueue::EnqueueStatus::Ok);

  const u8 small[7] = {};
  void* ptr = nullptr;
  CHECK(queue.reserve(sizeof(small), &ptr, 8) ==
        SpscQueue::EnqueueStatus::Dropped);
  CHECK(queue.enqueue(small, sizeof(small), 8) ==
        SpscQueue::EnqueueStatus::Dropped);

  // Admitting the record would have advanced the producer past the consumer,
  // so the bytes that are already in the ring must still read back intact.
  std::vector<u8> out(fill.size());
  REQUIRE(queue.dequeue(out.data(), out.size()) ==
          SpscQueue::DequeueStatus::Ok);
  CHECK(out == fill);
}

TEST_CASE("SpscQueue dequeue accounts for alignment padding",
          "[SpscQueueTest]") {
  SpscQueue queue;
  queue.init(4096);

  // Leave the consumer one byte into the ring, so a record aligned to eight
  // needs seven bytes of padding.
  const u8 first = 0x5A;
  REQUIRE(queue.enqueue(&first, 1) == SpscQueue::EnqueueStatus::Ok);
  u8 first_out = 0;
  REQUIRE(queue.dequeue(&first_out, 1) == SpscQueue::DequeueStatus::Ok);
  CHECK(first_out == first);

  // Fifteen bytes are readable, but an aligned fifteen byte record needs 22.
  const u8 filler[15] = {};
  REQUIRE(queue.enqueue(filler, sizeof(filler)) ==
          SpscQueue::EnqueueStatus::Ok);

  u8 out[15] = {};
  CHECK(queue.dequeue(out, sizeof(out), 8) == SpscQueue::DequeueStatus::Empty);
}

TEST_CASE("SpscQueue aligns records to the requested boundary",
          "[SpscQueueTest]") {
  SpscQueue queue;
  queue.init();

  const u8 first = 1;
  const u8 second = 2;
  REQUIRE(queue.enqueue(&first, 1) == SpscQueue::EnqueueStatus::Ok);
  REQUIRE(queue.enqueue(&second, 1, 8) == SpscQueue::EnqueueStatus::Ok);

  u8 first_out = 0;
  REQUIRE(queue.dequeue(&first_out, 1) == SpscQueue::DequeueStatus::Ok);
  CHECK(first_out == first);

  const char* const peeked = queue.peek(1, 8);
  CHECK(reinterpret_cast<uintptr_t>(peeked) % 8 == 0);
  CHECK(*peeked == static_cast<char>(second));
  queue.discard(1, 8);

  CHECK(queue.empty());
}

}  // namespace

}  // namespace container
