// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/mem/concurrent_arena.h"

#include <cstddef>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/mem/arena_ptr.h"
#include "fpag/mem/page_allocator.h"

namespace mem {

TEST_CASE("ConcurrentArena basic allocation and alignment", "[mem][arena]") {
  ConcurrentArena arena;

  SECTION("Initial state is zeroed") {
    CHECK(arena.capacity() == 0);
    CHECK(arena.size() == 0);
    CHECK(arena.committed_size() == 0);
  }

  SECTION("Reserve") {
    arena.reserve(page_size());
    CHECK(arena.capacity() == page_size());
    CHECK(arena.size() == 0);
    CHECK(arena.committed_size() == 0);
  }

  SECTION("Pages are committed lazily, a page at a time") {
    arena.reserve(page_size() * 4);
    void* const first = arena.alloc(1);
    REQUIRE(first != nullptr);
    CHECK(arena.committed_size() == page_size());

    void* const second = arena.alloc(page_size());
    REQUIRE(second != nullptr);
    CHECK(arena.committed_size() == page_size() * 2);
  }

  SECTION("Allocations are aligned and distinct") {
    arena.reserve(page_size());

    void* const ptr1 = arena.alloc(1);
    void* const ptr2 = arena.alloc(1);
    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);
    CHECK(ptr1 != ptr2);
    CHECK(reinterpret_cast<uintptr_t>(ptr1) % alignof(std::max_align_t) == 0);
    CHECK(reinterpret_cast<uintptr_t>(ptr2) % alignof(std::max_align_t) == 0);

    void* const aligned = arena.alloc(10, 64);
    REQUIRE(aligned != nullptr);
    CHECK(reinterpret_cast<uintptr_t>(aligned) % 64 == 0);
  }
}

TEST_CASE("ConcurrentArena object creation", "[mem][arena]") {
  ConcurrentArena arena;
  arena.reserve(page_size());

  SECTION("create<T> for i32") {
    i32* const value = arena.create<i32>(10);
    REQUIRE(value != nullptr);
    CHECK(*value == 10);
  }

  SECTION("create_managed<T> for non-trivial types") {
    static bool destroyed = false;
    struct NonTrivial {
      NonTrivial() { destroyed = false; }
      ~NonTrivial() { destroyed = true; }
    };

    {
      const ArenaUniquePtr<NonTrivial> ptr = arena.create_managed<NonTrivial>();
      CHECK(!destroyed);
    }
    CHECK(destroyed);
  }
}

TEST_CASE("ConcurrentArena move semantics", "[mem][arena]") {
  SECTION("Move constructor") {
    ConcurrentArena source;
    source.reserve(page_size());
    REQUIRE(source.alloc(1024) != nullptr);
    const usize size_before = source.size();

    ConcurrentArena moved(std::move(source));
    CHECK(moved.size() == size_before);
    CHECK(moved.capacity() == page_size());
  }

  SECTION("Move assignment releases the destination") {
    ConcurrentArena source;
    source.reserve(page_size());
    REQUIRE(source.alloc(1024) != nullptr);
    const usize size_before = source.size();

    ConcurrentArena destination;
    destination.reserve(page_size() * 2);
    destination = std::move(source);

    CHECK(destination.size() == size_before);
    CHECK(destination.capacity() == page_size());
  }
}

TEST_CASE("ConcurrentArena is usable from several threads",
          "[mem][arena][threads]") {
  // Every thread writes its own pattern into the block it just received. A
  // block that is advertised as committed but was not actually made writable
  // yet is written to here, and the write faults.
  constexpr usize THREADS = 8;
  constexpr usize ALLOCATIONS_PER_THREAD = 512;
  constexpr usize BLOCK_SIZE = 64;
  constexpr usize TOTAL_BLOCKS = THREADS * ALLOCATIONS_PER_THREAD;

  ConcurrentArena arena;
  arena.reserve(base::round_up(TOTAL_BLOCKS * BLOCK_SIZE + page_size() * 16,
                               page_size()));

  std::vector<u8*> blocks(TOTAL_BLOCKS, nullptr);

  const auto worker = [&arena, &blocks](usize thread_index) {
    const u8 pattern = static_cast<u8>(thread_index + 1);
    for (usize i = 0; i < ALLOCATIONS_PER_THREAD; ++i) {
      u8* const block = static_cast<u8*>(arena.alloc(BLOCK_SIZE, 1));
      // No assertion in here: Catch2's assertion machinery is not thread-safe,
      // and a null block shows up as a null entry checked after the join.
      if (block == nullptr) {
        return;
      }
      for (usize j = 0; j < BLOCK_SIZE; ++j) {
        block[j] = pattern;
      }
      blocks[thread_index * ALLOCATIONS_PER_THREAD + i] = block;
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(THREADS);
  for (usize t = 0; t < THREADS; ++t) {
    threads.emplace_back(worker, t);
  }
  for (std::thread& thread : threads) {
    thread.join();
  }

  const auto block_matches = [](const u8* block, u8 expected) {
    for (usize i = 0; i < BLOCK_SIZE; ++i) {
      if (block[i] != expected) {
        return false;
      }
    }
    return true;
  };

  for (usize t = 0; t < THREADS; ++t) {
    const u8 pattern = static_cast<u8>(t + 1);
    for (usize i = 0; i < ALLOCATIONS_PER_THREAD; ++i) {
      const u8* const block = blocks[t * ALLOCATIONS_PER_THREAD + i];
      REQUIRE(block != nullptr);
      CHECK(block_matches(block, pattern));
    }
  }
}

#if !FPAG_BUILD_FLAG(IS_DEBUG)
TEST_CASE("ConcurrentArena reports exhaustion instead of constructing at null",
          "[mem][arena]") {
  // See the matching Arena test: only reachable in release.
  ConcurrentArena arena;
  arena.reserve(page_size());
  REQUIRE(arena.alloc(page_size()) != nullptr);
  CHECK(arena.create<u64>(u64{1}) == nullptr);
}
#endif

}  // namespace mem
