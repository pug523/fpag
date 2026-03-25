// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/mem/page_allocator.h"

#include "base/numeric.h"
#include "catch2/catch_test_macros.hpp"

namespace base {

TEST_CASE("Page allocation and lifecycle", "[base][memory]") {
  const usize size = kPageSize * 4;

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
    bool success = commit_pages(ptr, size);
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
    void* ptr = allocate_huge_pages(kHugePageSize);
    CHECK(ptr != nullptr);
    free_pages(ptr, kHugePageSize);
  }
}

TEST_CASE("Constants validation", "[base][memory]") {
  // Ensure constants match expected architectural values
  CHECK(kPageSize == 4096);
  CHECK(kHugePageSize == 2097152);  // 2 MiB
}

}  // namespace base
