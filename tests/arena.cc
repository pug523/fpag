// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "mem/arena.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/numeric.h"
#include "catch2/catch_test_macros.hpp"
#include "mem/arena_ptr.h"
#include "mem/page_allocator.h"

namespace mem {

TEST_CASE("Arena basic allocation and alignment", "[base][arena]") {
  Arena arena;

  SECTION("Initial state is zeroed") {
    CHECK(arena.capacity() == 0);
    CHECK(arena.size() == 0);
  }

  SECTION("Reserve") {
    arena.reserve(kPageSize);
    CHECK(arena.capacity() == kPageSize);
    CHECK(arena.size() == 0);
  }

  SECTION("Small allocation alignment") {
    arena.reserve(kPageSize);

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
    arena.reserve(kPageSize);

    // Preservation with 64 B alignment
    void* ptr = arena.alloc(10, 64);
    CHECK(ptr != nullptr);
    CHECK(reinterpret_cast<uintptr_t>(ptr) % 64 == 0);
  }
}

TEST_CASE("Arena object creation", "[base][arena]") {
  Arena arena;
  arena.reserve(kPageSize);

  SECTION("create<T> for i32") {
    i32* i = arena.create<i32>(10);
    REQUIRE(i != nullptr);
    CHECK(*i == 10);
  }

  struct Trivial {
    int x;
    float y;
  };

  SECTION("create<T> for trivial struct") {
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

TEST_CASE("Arena move semantics", "[base][arena]") {
  SECTION("Move constructor") {
    Arena arena1;
    arena1.reserve(kPageSize);
    void* p = arena1.alloc(1024);
    CHECK(p != nullptr);
    const usize size_before = arena1.size();
    Arena moved_arena(std::move(arena1));
    CHECK(moved_arena.size() == size_before);
  }

  SECTION("Move assignment") {
    Arena arena2;
    arena2.reserve(kPageSize);
    void* p = arena2.alloc(1024);
    CHECK(p != nullptr);

    Arena new_arena;
    const usize size_before = arena2.size();
    new_arena = std::move(arena2);
    CHECK(new_arena.size() == size_before);
  }
}

TEST_CASE("Arena edge cases", "[base][arena]") {
  Arena arena;
  arena.reserve(64ull * 1024 * 1024 * 1024);  // 64 GiB

  SECTION("Very large capacity") {
    void* p = arena.alloc(3ull * 1024 * 1024 * 1024);  // 3 GiB
    CHECK(p);
  }
}

}  // namespace mem
