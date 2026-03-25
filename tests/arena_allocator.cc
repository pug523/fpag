// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/arena_allocator.h"

#include <type_traits>
#include <utility>
#include <vector>

#include "base/numeric.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_all.hpp"

namespace base {

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
    (void)ptr1;

    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr1);
    CHECK(addr % alignof(std::max_align_t) == 0);

    void* ptr2 = arena.alloc(1);
    uintptr_t addr2 = reinterpret_cast<uintptr_t>(ptr2);
    CHECK(addr2 % alignof(std::max_align_t) == 0);
    CHECK(ptr1 != ptr2);
  }

  SECTION("Custom alignment requirements") {
    // Preservation with 64 byte alignment
    void* ptr = arena.alloc(10, false, 64);
    CHECK(reinterpret_cast<uintptr_t>(ptr) % 64 == 0);
  }

  SECTION("Allocation size zero") {
    // Should return a valid pointer or nullptr for 0 size allocation
    void* _ = arena.alloc(0);
  }
}

TEST_CASE("ArenaAllocator block management", "[base][arena]") {
  ArenaAllocator arena;

  SECTION("Large allocation triggering new blocks") {
    const usize first_alloc_size = 2 * 1024 * 1024;  // 2 MiB
    void* p1 = arena.alloc(first_alloc_size);
    auto pos1 = arena.current_position();
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
    void* _ = arena.alloc(100);
    _ = arena.alloc(ArenaAllocator::kBlockSize);
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
    auto* obj = arena.create<Trivial>(10, 2.5f);
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
      auto ptr = arena.create_managed<NonTrivial>();
      CHECK(!destroyed);
    }
    // Should be destroyed when arena goes out of scope.
    CHECK(destroyed);
  }
}

TEST_CASE("ArenaAllocator move semantics", "[base][arena]") {
  ArenaAllocator arena;
  void* _ = arena.alloc(1024);
  ArenaAllocator::BlockPosition pos_before = arena.current_position();

  SECTION("Move constructor") {
    ArenaAllocator moved_arena(std::move(arena));
    CHECK(moved_arena.current_position().offset == pos_before.offset);
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
    void* p = arena.alloc(3ULL * 1024 * 1024 * 1024);  // 3 GiB
    CHECK(p != nullptr);
  }

  SECTION("Huge pages flag") {
    void* p = arena.alloc(1024, true);
    CHECK(p != nullptr);
  }
}

}  // namespace base
