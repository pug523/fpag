// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/mem/page_allocator.h"

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

namespace mem {

TEST_CASE("Page allocation and lifecycle", "[base][memory]") {
  const usize size = page_size() * 4;

  SECTION("Standard page allocation") {
    // Test basic allocation and deallocation.
    void* ptr = allocate_pages(size);

    REQUIRE(ptr != nullptr);

    // Verify we can write to the memory (this would segfault if allocation
    // failed).
    u8* byte_ptr = static_cast<u8*>(ptr);
    byte_ptr[0] = 0xAA;
    byte_ptr[size - 1] = 0xBB;

    CHECK(byte_ptr[0] == 0xAA);
    CHECK(byte_ptr[size - 1] == 0xBB);

    free_pages(ptr, size);
  }

  SECTION("Reserve and Commit workflow") {
    // Reserve address space without backing it with physical memory.
    void* ptr = reserve_pages(size);
    REQUIRE(ptr != nullptr);

    // Commit the pages to make them usable.
    const bool success = commit_pages(ptr, size);
    CHECK(success);

    // Test accessibility.
    u8* byte_ptr = static_cast<u8*>(ptr);
    byte_ptr[0] = 0xCC;
    CHECK(byte_ptr[0] == 0xCC);

    // Decommit memory (returns physical memory to OS but keeps
    // reservation).
    decommit_pages(ptr, size);

    // Final cleanup.
    free_pages(ptr, size);
  }

  SECTION("Huge page allocation") {
    void* ptr = allocate_huge_pages(huge_page_size());
    CHECK(ptr != nullptr);
    free_pages(ptr, huge_page_size());
  }
}

TEST_CASE("Aliased pages map the region twice", "[base][memory]") {
  const usize size = page_size();

#if FPAG_BUILD_FLAG(IS_OS_ASMJS)
  // The wasm runtime has no way to map one region twice; the function says so
  // by returning null instead of a broken mapping.
  CHECK(allocate_aliased_pages(size) == nullptr);
#else
  u8* const base = static_cast<u8*>(allocate_aliased_pages(size));
  REQUIRE(base != nullptr);

  // The second half has to alias the first. That alias is what lets a record
  // that crosses the end of a ring be handled as one contiguous run.
  for (usize i = 0; i < size; ++i) {
    base[i] = static_cast<u8>(i % 251);
  }
  for (usize i = 0; i < size; ++i) {
    CHECK(base[size + i] == static_cast<u8>(i % 251));
  }

  // Writing through the alias has to be visible in the first half too.
  base[size + 3] = 0xEE;
  CHECK(base[3] == 0xEE);

  free_aliased_pages(base, size);
#endif
}

}  // namespace mem
