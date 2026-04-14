// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstdint>

#include "base/numeric.h"

namespace mem {

// Reserves a contiguous block of virtual memory for the given size without
// committing any pages.
[[nodiscard]] void* reserve_pages(usize size);

// Commits a contiguous block of virtual memory.
bool commit_pages(void* ptr, usize size);

// Decommits a contiguous block of virtual memory.
void decommit_pages(void* ptr, usize size);

// Allocates a contiguous block of virtual memory for the given size.
// Calls reserve_pages() and commit_pages() internally.
[[nodiscard]] void* allocate_pages(usize size);

// Allocates a contiguous block of virtual memory for the given size using
// huge pages.
[[nodiscard]] void* allocate_huge_pages(usize size);

// Allocates circular mapped pages for the given size for easy wrap around
// indexing.
[[nodiscard]] void* allocate_aliased_pages(usize size);

// Frees a contiguous block of virtual memory.
// `size` is not used on Windows, but is required on POSIX systems.
void free_pages(void* ptr, usize size);

static constexpr u64 kPageSize = 4096;                 // 4 KiB
static constexpr u64 kHugePageSize = 2 * 1024 * 1024;  // 2 MiB

inline bool is_page_aligned_ptr(void* ptr) {
  return reinterpret_cast<uintptr_t>(ptr) % kPageSize == 0;
}

inline bool is_page_aligned_size(usize size) {
  return size % kPageSize == 0;
}

inline bool is_huge_page_aligned_size(usize size) {
  return size % kHugePageSize == 0;
}

}  // namespace mem
