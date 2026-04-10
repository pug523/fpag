// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/spsc_queue.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/numeric.h"
#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

namespace base {

namespace {

TEST_CASE("SpscQueue Constructor and Capacity", "[SpscQueueTest]") {
  SpscQueue queue_small;
  queue_small.init();
  CHECK(queue_small.capacity() == SpscQueue::kDefaultCapacity);
  CHECK(queue_small.empty());
  CHECK(queue_small.size() == 0);
  CHECK(queue_small.available() == SpscQueue::kDefaultCapacity);

  SpscQueue queue_large;
  queue_large.init(SpscQueue::kDefaultCapacity * 1024);
  CHECK(queue_large.capacity() == SpscQueue::kDefaultCapacity * 1024);
  CHECK(queue_large.empty());
  CHECK(queue_large.size() == 0);
  CHECK(queue_large.available() == SpscQueue::kDefaultCapacity * 1024);
}

TEST_CASE("SpscQueue Enqueue Dequeue Single Element", "[SpscQueueTest]") {
  SpscQueue queue;
  queue.init();

  SECTION(
      "Manual enqueue(reserve/commit) and dequeue(peek/discard) single int") {
    constexpr i32 kDataIn = 42;

    void* ptr = nullptr;
    SpscQueue::EnqueueStatus result = queue.reserve(sizeof(kDataIn), &ptr);
    CHECK(result == SpscQueue::EnqueueStatus::kOk);
    *static_cast<i32*>(ptr) = kDataIn;
    queue.commit(sizeof(kDataIn));
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(kDataIn));
    CHECK(queue.available() == SpscQueue::kDefaultCapacity - sizeof(kDataIn));

    const char* peeked = queue.peek(sizeof(kDataIn));
    const i32* data_out_ptr = reinterpret_cast<const i32*>(peeked);
    const i32 data_out_copied = *data_out_ptr;
    queue.discard(sizeof(data_out_copied));
    CHECK(queue.empty());
    CHECK(queue.size() == 0);
    CHECK(queue.available() == SpscQueue::kDefaultCapacity);
    CHECK(data_out_copied == kDataIn);
  }

  SECTION(
      "Manual enqueue(reserve/commit) and dequeue(peek/discard) single char") {
    constexpr char kCharIn = 'A';

    void* ptr = nullptr;
    SpscQueue::EnqueueStatus result = queue.reserve(sizeof(kCharIn), &ptr);
    CHECK(result == SpscQueue::EnqueueStatus::kOk);
    *static_cast<char*>(ptr) = kCharIn;
    queue.commit(sizeof(kCharIn));
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(kCharIn));
    CHECK(queue.available() == SpscQueue::kDefaultCapacity - sizeof(kCharIn));

    const char char_out =
        *reinterpret_cast<const char*>(queue.peek(sizeof(kCharIn)));
    queue.discard(sizeof(kCharIn));
    CHECK(queue.empty());
    CHECK(char_out == kCharIn);
  }

  SECTION("Automatic enqueue and dequeue single int") {
    constexpr i32 kDataIn = 42;

    SpscQueue::EnqueueStatus result = queue.enqueue(&kDataIn, sizeof(kDataIn));
    CHECK(result == SpscQueue::EnqueueStatus::kOk);
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(kDataIn));
    CHECK(queue.available() == SpscQueue::kDefaultCapacity - sizeof(kDataIn));

    i32 data_out;
    queue.dequeue(static_cast<void*>(&data_out), sizeof(data_out));
    CHECK(queue.empty());
    CHECK(queue.size() == 0);
    CHECK(queue.available() == SpscQueue::kDefaultCapacity);
    CHECK(data_out == kDataIn);
  }

  SECTION("Automatic enqueue and dequeue single char") {
    constexpr char kCharIn = 'A';

    SpscQueue::EnqueueStatus result = queue.enqueue(&kCharIn, sizeof(kCharIn));
    CHECK(result == SpscQueue::EnqueueStatus::kOk);
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(kCharIn));
    CHECK(queue.available() == SpscQueue::kDefaultCapacity - sizeof(kCharIn));

    char char_out;
    queue.dequeue(&char_out, sizeof(kCharIn));
    CHECK(queue.empty());
    CHECK(char_out == kCharIn);
  }
}

}  // namespace

}  // namespace base
