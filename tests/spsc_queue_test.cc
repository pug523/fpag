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

}  // namespace

}  // namespace container
