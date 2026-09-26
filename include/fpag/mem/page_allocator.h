// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstdint>

#include "fpag/base/numeric.h"

namespace mem {

/// Reserves a contiguous block of virtual memory for the given size without
/// committing any pages.
[[nodiscard]] void* reserve_pages(usize size);

/// Commits a contiguous block of virtual memory.
bool commit_pages(void* ptr, usize size);

/// Decommits a contiguous block of virtual memory.
void decommit_pages(void* ptr, usize size);

/// Allocates a contiguous block of virtual memory for the given size.
/// Calls reserve_pages() and commit_pages() internally.
[[nodiscard]] void* allocate_pages(usize size);

/// Allocates a contiguous block of virtual memory for the given size using
/// huge pages.
[[nodiscard]] void* allocate_huge_pages(usize size);

/// Allocates a circular mapping of `size` bytes: `2 * size` bytes of address
/// space are reserved, and the second `size` bytes alias the first, so any
/// index in `[0, 2 * size)` addresses the ring without a branch.
/// Returns nullptr, and never a plain allocation, when the aliasing cannot be
/// set up: a caller that assumes contiguity across the wrap would otherwise
/// write out of bounds.
[[nodiscard]] void* allocate_aliased_pages(usize size);

/// Frees a contiguous block of virtual memory.
/// `size` is not used on Windows, but is required on POSIX systems.
void free_pages(void* ptr, usize size);

/// Frees a block returned by allocate_aliased_pages(), of the same `size` that
/// was passed to it.
void free_aliased_pages(void* ptr, usize size);

usize page_size();

usize huge_page_size();

usize mmap_alignment();

inline bool is_page_aligned_ptr(void* ptr) {
  return reinterpret_cast<uintptr_t>(ptr) % page_size() == 0;
}

inline bool is_page_aligned_size(usize size) {
  return size % page_size() == 0;
}

inline bool is_huge_page_aligned_size(usize size) {
  return size % huge_page_size() == 0;
}

inline bool is_mmap_aligned_size(usize size) {
  return size % mmap_alignment() == 0;
}

}  // namespace mem
