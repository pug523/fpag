// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/mem/arena.h"

#include <utility>

#include "fpag/base/debug/check.h"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/mem/page_allocator.h"

namespace mem {

Arena::Arena(Arena&& other) noexcept
    : ptr_(std::exchange(other.ptr_, nullptr)),
      capacity_(std::exchange(other.capacity_, 0)),
      size_(std::exchange(other.size_, 0)),
      committed_size_(std::exchange(other.committed_size_, 0)) {}

Arena& Arena::operator=(Arena&& other) noexcept {
  ptr_ = std::exchange(other.ptr_, nullptr);
  capacity_ = std::exchange(other.capacity_, 0);
  size_ = std::exchange(other.size_, 0);
  committed_size_ = std::exchange(other.committed_size_, 0);
  return *this;
}

void Arena::reserve(usize capacity) {
  FPAG_DCHECK_MSG(!ptr_, "Arena is already reserved.");

  capacity_ = capacity;
  FPAG_DCHECK_MSG(is_page_aligned_size(capacity_),
                  "Capacity must be page aligned.");

  ptr_ = static_cast<char*>(reserve_pages(capacity_));
  FPAG_DCHECK_MSG(ptr_, "Failed to reserve pages for arena.");
}

void Arena::reset() {
  FPAG_DCHECK(ptr_);

  free_pages(ptr_, capacity_);
  ptr_ = nullptr;
  capacity_ = 0;
  size_ = 0;
  committed_size_ = 0;
}

void* Arena::alloc(usize size, usize align) {
  FPAG_DCHECK(ptr_);

  const usize current_offset = base::round_up(size_, align);
  const usize next_offset = current_offset + size;
  FPAG_DCHECK_LE_MSG(next_offset, capacity_, "Arena capacity exceeded.");

  commit_until(next_offset);

  size_ = next_offset;
  return ptr_ + current_offset;
}

void Arena::commit_until(usize end_offset) {
  FPAG_DCHECK(ptr_);

  FPAG_DCHECK_LE(end_offset, capacity_);
  if (end_offset <= committed_size_) {
    return;
  }

  usize target_end = base::round_up(end_offset, kPageSize);
  if (target_end > capacity_) {
    target_end = capacity_;
  }
  const usize commit_diff = target_end - committed_size_;
  FPAG_DCHECK_GT(commit_diff, 0ull);
  char* const commit_ptr = ptr_ + committed_size_;
  if (!commit_pages(commit_ptr, commit_diff)) [[unlikely]] {
    FPAG_DCHECK_MSG(false, "Failed to commit pages for arena.");
    return;
  }
  committed_size_ = target_end;
}

}  // namespace mem
