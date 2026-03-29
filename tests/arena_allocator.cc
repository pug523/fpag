// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "mem/arena_allocator.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/numeric.h"
#include "catch2/catch_test_macros.hpp"
#include "mem/arena_deleter.h"

namespace mem {

TEST_CASE("ArenaAllocator basic allocation and alignment", "[base][arena]") {
  ArenaAllocator arena;

  SECTION("Initial state is zeroed") {
    ArenaAllocator::BlockPosition pos = arena.current_position();
    CHECK(pos.block_id == 0);
    CHECK(pos.offset == 0);
  }

  SECTION("Small allocation alignment") {
    // Preservation with default alignment
    void* ptr1 = arena.alloc(1);

    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr1);
    CHECK(addr % alignof(std::max_align_t) == 0);

    void* ptr2 = arena.alloc(1);
    uintptr_t addr2 = reinterpret_cast<uintptr_t>(ptr2);
    CHECK(addr2 % alignof(std::max_align_t) == 0);
    CHECK(ptr1 != ptr2);
  }

  SECTION("Custom alignment requirements") {
    // Preservation with 64 B alignment
    void* ptr = arena.alloc(10, false, 64);
    CHECK(ptr != nullptr);
    CHECK(reinterpret_cast<uintptr_t>(ptr) % 64 == 0);
  }
}

TEST_CASE("ArenaAllocator block management", "[base][arena]") {
  ArenaAllocator arena;

  SECTION("Large allocation triggering new blocks") {
    const usize first_alloc_size = 2 * 1024 * 1024;  // 2 MiB
    void* p1 = arena.alloc(first_alloc_size);
    ArenaAllocator::BlockPosition pos1 = arena.current_position();
    CHECK(pos1.block_id == 0);

    // Allocating beyond the first block should increment the block ID
    void* p2 = arena.alloc(128);
    ArenaAllocator::BlockPosition pos2 = arena.current_position();

    CHECK(p1 != nullptr);
    CHECK(p2 != nullptr);
    CHECK(pos2.block_id > pos1.block_id);
  }

  SECTION("Reserve allocates capacity upfront") {
    arena.reserve(10 * 1024 * 1024);
    ArenaAllocator::BlockPosition pos = arena.current_position();

    usize expected_metadata_size =
        sizeof(ArenaAllocator::Block*) * ArenaAllocator::kMaxBlocks;
    CHECK(pos.offset == expected_metadata_size);
    CHECK(pos.block_id == 0);
  }

  SECTION("Reset clears the state") {
    void* ptr = arena.alloc(100);
    CHECK(ptr != nullptr);
    ptr = arena.alloc(ArenaAllocator::kBlockSize);
    CHECK(ptr != nullptr);
    arena.reset();

    ArenaAllocator::BlockPosition pos = arena.current_position();
    CHECK(pos.block_id == 0);
    CHECK(pos.offset == 0);
  }
}

TEST_CASE("ArenaAllocator object creation", "[base][arena]") {
  ArenaAllocator arena;

  struct Trivial {
    int x;
    float y;
  };

  SECTION("create<T> for trivial types") {
    Trivial* obj = arena.create<Trivial>(10, 2.5f);
    REQUIRE(obj != nullptr);
    CHECK(obj->x == 10);
    CHECK(obj->y == 2.5f);
    // No destructor needed for trivial types.
  }

  SECTION("create_managed<T> for non-trivial types") {
    static bool destroyed = false;
    struct NonTrivial {
      NonTrivial() { destroyed = false; }
      ~NonTrivial() { destroyed = true; }
    };

    {
      ArenaUniquePtr<NonTrivial> ptr = arena.create_managed<NonTrivial>();
      CHECK(!destroyed);
    }
    // Should be destroyed when arena goes out of scope.
    CHECK(destroyed);
  }
}

TEST_CASE("ArenaAllocator move semantics", "[base][arena]") {
  ArenaAllocator arena;
  void* p = arena.alloc(1024);
  CHECK(p != nullptr);
  ArenaAllocator::BlockPosition pos_before = arena.current_position();

  SECTION("Move constructor") {
    ArenaAllocator moved_arena(std::move(arena));
    CHECK(moved_arena.current_position().offset == pos_before.offset);
    // NOLINTNEXTLINE(bugprone-use-after-move)
    CHECK(arena.current_position().block_id == 0);
  }

  SECTION("Move assignment") {
    ArenaAllocator new_arena;
    new_arena = std::move(arena);
    CHECK(new_arena.current_position().offset == pos_before.offset);
  }
}

TEST_CASE("ArenaAllocator edge cases and limits", "[base][arena]") {
  ArenaAllocator arena;

  SECTION("Very large capacity") {
    void* p = arena.alloc(3ull * 1024 * 1024 * 1024);  // 3 GiB
    CHECK(p != nullptr);
  }

  SECTION("Huge pages flag") {
    void* p = arena.alloc(1024, true);
    CHECK(p != nullptr);
  }
}

}  // namespace mem
