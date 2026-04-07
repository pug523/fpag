// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "mem/arena.h"

#include <utility>

#include "base/debug/check.h"
#include "base/math_util.h"
#include "base/numeric.h"
#include "mem/page_allocator.h"

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
  dcheck_msg(!ptr_, "Arena is already reserved.");

  capacity_ = capacity;
  dcheck_msg(is_page_aligned_size(capacity_), "Capacity must be page aligned.");

  ptr_ = static_cast<char*>(reserve_pages(capacity_));
  dcheck_msg(ptr_, "Failed to reserve pages for arena.");
}

void Arena::reset() {
  dcheck(ptr_);

  free_pages(ptr_, capacity_);
  ptr_ = nullptr;
  capacity_ = 0;
  size_ = 0;
  committed_size_ = 0;
}

void* Arena::alloc(usize size, usize align) {
  dcheck(ptr_);

  const usize current_offset = base::round_up(size_, align);
  const usize next_offset = current_offset + size;
  dcheck_le_msg(next_offset, capacity_, "Arena capacity exceeded.");

  commit_until(next_offset);

  size_ = next_offset;
  return ptr_ + current_offset;
}

void Arena::commit_until(usize end_offset) {
  dcheck(ptr_);

  dcheck_le(end_offset, capacity_);
  if (end_offset <= committed_size_) {
    return;
  }

  usize target_end = base::round_up(end_offset, kPageSize);
  if (target_end > capacity_) {
    target_end = capacity_;
  }
  const usize commit_diff = target_end - committed_size_;
  dcheck_gt(commit_diff, 0ull);
  char* const commit_ptr = ptr_ + committed_size_;
  if (!commit_pages(commit_ptr, commit_diff)) [[unlikely]] {
    dcheck_msg(false, "Failed to commit pages for arena.");
    return;
  }
  committed_size_ = target_end;
}

}  // namespace mem
