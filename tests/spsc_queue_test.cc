// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/container/spsc_queue.h"

#include <atomic>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/mem/page_allocator.h"

namespace container {

namespace {

// The ring is mapped a whole number of pages, and a page is 4 KiB on Linux and
// Windows but 16 KiB on Apple silicon, so the cases that need a small ring take
// its size from the allocator rather than from a literal.
usize ring_bytes() {
  return mem::page_size();
}

TEST_CASE("SpscQueue Constructor and Capacity",
          "[SpscQueueTest][aliased_pages]") {
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

TEST_CASE("SpscQueue Enqueue Dequeue Single Element",
          "[SpscQueueTest][aliased_pages]") {
  SpscQueue queue;
  queue.init();

  SECTION(
      "Manual enqueue(reserve/commit) and dequeue(peek/discard) single "
      "integer") {
    constexpr i32 DATA_IN = 42;

    void* ptr = nullptr;
    const SpscQueue::EnqueueStatus result =
        queue.reserve(sizeof(DATA_IN), &ptr);
    CHECK(result == SpscQueue::EnqueueStatus::Ok);
    *static_cast<i32*>(ptr) = DATA_IN;
    queue.commit(sizeof(DATA_IN));
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(DATA_IN));
    CHECK(queue.available() == SpscQueue::default_capacity() - sizeof(DATA_IN));

    const char* peeked = queue.peek(sizeof(DATA_IN));
    const i32* data_out_ptr = reinterpret_cast<const i32*>(peeked);
    const i32 data_out_copied = *data_out_ptr;
    queue.discard(sizeof(data_out_copied));
    CHECK(queue.empty());
    CHECK(queue.size() == 0);
    CHECK(queue.available() == SpscQueue::default_capacity());
    CHECK(data_out_copied == DATA_IN);
  }

  SECTION(
      "Manual enqueue(reserve/commit) and dequeue(peek/discard) single char") {
    constexpr char CHAR_IN = 'A';

    void* ptr = nullptr;
    const SpscQueue::EnqueueStatus result =
        queue.reserve(sizeof(CHAR_IN), &ptr);
    CHECK(result == SpscQueue::EnqueueStatus::Ok);
    *static_cast<char*>(ptr) = CHAR_IN;
    queue.commit(sizeof(CHAR_IN));
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(CHAR_IN));
    CHECK(queue.available() == SpscQueue::default_capacity() - sizeof(CHAR_IN));

    const char char_out =
        *reinterpret_cast<const char*>(queue.peek(sizeof(CHAR_IN)));
    queue.discard(sizeof(CHAR_IN));
    CHECK(queue.empty());
    CHECK(char_out == CHAR_IN);
  }

  SECTION("Automatic enqueue and dequeue single integer") {
    constexpr i32 DATA_IN = 42;

    const SpscQueue::EnqueueStatus result =
        queue.enqueue(&DATA_IN, sizeof(DATA_IN));
    CHECK(result == SpscQueue::EnqueueStatus::Ok);
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(DATA_IN));
    CHECK(queue.available() == SpscQueue::default_capacity() - sizeof(DATA_IN));

    i32 data_out = 0;
    queue.dequeue(static_cast<void*>(&data_out), sizeof(data_out));
    CHECK(queue.empty());
    CHECK(queue.size() == 0);
    CHECK(queue.available() == SpscQueue::default_capacity());
    CHECK(data_out == DATA_IN);
  }

  SECTION("Automatic enqueue and dequeue single char") {
    constexpr char CHAR_IN = 'A';

    const SpscQueue::EnqueueStatus result =
        queue.enqueue(&CHAR_IN, sizeof(CHAR_IN));
    CHECK(result == SpscQueue::EnqueueStatus::Ok);
    CHECK_FALSE(queue.empty());
    CHECK(queue.size() == sizeof(CHAR_IN));
    CHECK(queue.available() == SpscQueue::default_capacity() - sizeof(CHAR_IN));

    char char_out = 0;
    queue.dequeue(&char_out, sizeof(CHAR_IN));
    CHECK(queue.empty());
    CHECK(char_out == CHAR_IN);
  }
}

TEST_CASE("SpscQueue hands a record from one thread to another",
          "[SpscQueueTest][threads][aliased_pages]") {
  // The consumer has to observe the record's payload, not only the counter:
  // size_consumer() pairs with the producer's release store, and that pairing
  // is what orders the consumer's reads after the producer's writes. With a
  // relaxed load the two race, which TSan reports for this case.
  constexpr usize RECORDS = 4096;
  constexpr usize RECORD_SIZE = 64;

  SpscQueue queue;
  queue.init(SpscQueue::default_capacity(), SpscQueue::Mode::Block);

  std::atomic<bool> mismatch{false};

  std::thread consumer([&queue, &mismatch] {
    std::vector<u8> out(RECORD_SIZE);
    for (usize record = 0; record < RECORDS; ++record) {
      SpscQueue::DequeueStatus status = SpscQueue::DequeueStatus::Empty;
      do {
        status = queue.dequeue(out.data(), out.size());
        if (status != SpscQueue::DequeueStatus::Ok) {
          std::this_thread::yield();
        }
      } while (status != SpscQueue::DequeueStatus::Ok);

      for (usize i = 0; i < RECORD_SIZE; ++i) {
        if (out[i] != static_cast<u8>(record + i)) {
          mismatch.store(true);
        }
      }
    }
  });

  std::vector<u8> record(RECORD_SIZE);
  for (usize r = 0; r < RECORDS; ++r) {
    for (usize i = 0; i < RECORD_SIZE; ++i) {
      record[i] = static_cast<u8>(r + i);
    }

    const SpscQueue::EnqueueStatus status =
        queue.enqueue(record.data(), record.size());
    CHECK(status == SpscQueue::EnqueueStatus::Ok);
  }

  consumer.join();
  CHECK_FALSE(mismatch.load());
}

TEST_CASE("SpscQueue wraps a record across the end of the ring",
          "[SpscQueueTest][aliased_pages]") {
  SpscQueue queue;
  queue.init(ring_bytes());

  // Walk both ends of the ring to within a hundred bytes of the end, so the
  // next record has to continue past it. The circular mapping is what makes
  // that one contiguous run.
  std::vector<u8> fill(ring_bytes() - 96);
  for (usize i = 0; i < fill.size(); ++i) {
    fill[i] = static_cast<u8>(i % 251);
  }
  REQUIRE(queue.enqueue(fill.data(), fill.size()) ==
          SpscQueue::EnqueueStatus::Ok);

  std::vector<u8> drained(fill.size());
  REQUIRE(queue.dequeue(drained.data(), drained.size()) ==
          SpscQueue::DequeueStatus::Ok);
  CHECK(drained == fill);

  std::vector<u8> crossing(200);
  for (usize i = 0; i < crossing.size(); ++i) {
    crossing[i] = static_cast<u8>(i);
  }
  REQUIRE(queue.enqueue(crossing.data(), crossing.size()) ==
          SpscQueue::EnqueueStatus::Ok);

  std::vector<u8> out(crossing.size());
  REQUIRE(queue.dequeue(out.data(), out.size()) ==
          SpscQueue::DequeueStatus::Ok);
  CHECK(out == crossing);
}

TEST_CASE("SpscQueue refuses a record whose alignment padding does not fit",
          "[SpscQueueTest][aliased_pages]") {
  SpscQueue queue;
  queue.init(ring_bytes());

  // Leave seven bytes free, then ask for a seven byte record aligned to eight.
  // The payload fits, the seven bytes of padding in front of it do not.
  std::vector<u8> fill(ring_bytes() - 7);
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
          "[SpscQueueTest][aliased_pages]") {
  SpscQueue queue;
  queue.init(ring_bytes());

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
          "[SpscQueueTest][aliased_pages]") {
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

TEST_CASE("SpscQueue move assignment takes over the source's state",
          "[SpscQueueTest][aliased_pages]") {
  // The destination's own ring is released rather than overwritten. That leak
  // is not visible from here, so what this case pins down is the state
  // transfer: the records and the counters both belong to the moved queue.
  SpscQueue source;
  source.init(ring_bytes(), SpscQueue::Mode::Drop);
  const i32 value = 7;
  REQUIRE(source.enqueue(&value, sizeof(value)) ==
          SpscQueue::EnqueueStatus::Ok);

  // Fill the rest of the ring, then one more record has to be dropped.
  std::vector<u8> fill(ring_bytes() - sizeof(value));
  REQUIRE(source.enqueue(fill.data(), fill.size()) ==
          SpscQueue::EnqueueStatus::Ok);
  const u8 dropped = 0;
  CHECK(source.enqueue(&dropped, 1) == SpscQueue::EnqueueStatus::Dropped);
  REQUIRE(source.dropped_count() == 1);

  SpscQueue destination;
  destination.init();
  destination = std::move(source);

  CHECK(destination.capacity() == ring_bytes());
  CHECK(destination.dropped_count() == 1);
  i32 out = 0;
  REQUIRE(destination.dequeue(&out, sizeof(out)) ==
          SpscQueue::DequeueStatus::Ok);
  CHECK(out == value);

  CHECK(source.dropped_count() == 0);
  CHECK(source.capacity() == 0);
}

}  // namespace

}  // namespace container
