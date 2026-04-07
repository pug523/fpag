// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "mem/concurrent_arena.h"

#include <atomic>
#include <utility>

#include "base/debug/check.h"
#include "base/math_util.h"
#include "base/numeric.h"
#include "mem/page_allocator.h"

namespace mem {

ConcurrentArena::ConcurrentArena(ConcurrentArena&& other) noexcept
    : ptr_(std::exchange(other.ptr_, nullptr)),
      capacity_(std::exchange(other.capacity_, 0)),
      size_(other.size_.load(std::memory_order_relaxed)),
      committed_size_(other.committed_size_.load(std::memory_order_relaxed)) {
  other.size_.store(0, std::memory_order_relaxed);
  other.committed_size_.store(0, std::memory_order_relaxed);
}

ConcurrentArena& ConcurrentArena::operator=(ConcurrentArena&& other) noexcept {
  ptr_ = std::exchange(other.ptr_, nullptr);
  capacity_ = std::exchange(other.capacity_, 0);

  size_.store(other.size_.load(std::memory_order_relaxed),
              std::memory_order_relaxed);
  committed_size_.store(other.committed_size_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);

  other.size_.store(0, std::memory_order_relaxed);
  other.committed_size_.store(0, std::memory_order_relaxed);

  return *this;
}

void ConcurrentArena::reserve(usize capacity) {
  dcheck_msg(!ptr_, "Arena is already reserved.");

  capacity_ = capacity;
  dcheck_msg(is_page_aligned_size(capacity_), "Capacity must be page aligned.");

  ptr_ = static_cast<char*>(reserve_pages(capacity_));
  dcheck_msg(ptr_, "Failed to reserve pages for arena.");

  size_.store(0, std::memory_order_relaxed);
  committed_size_.store(0, std::memory_order_relaxed);
}

void ConcurrentArena::reset() {
  dcheck(ptr_);

  free_pages(ptr_, capacity_);
  ptr_ = nullptr;
  capacity_ = 0;

  size_.store(0, std::memory_order_relaxed);
  committed_size_.store(0, std::memory_order_relaxed);
}

void* ConcurrentArena::alloc(usize size, usize align) {
  dcheck(ptr_);

  usize old_size;
  usize new_size;

  // Bump pointer
  while (true) {
    old_size = size_.load(std::memory_order_relaxed);
    const usize aligned = base::round_up(old_size, align);
    new_size = aligned + size;

    if (new_size > capacity_) [[unlikely]] {
      dcheck_msg(false, "Arena capacity exceeded.");
      return nullptr;
    }

    if (size_.compare_exchange_weak(old_size, new_size,
                                    std::memory_order_acq_rel,
                                    std::memory_order_relaxed)) {
      // Success
      old_size = aligned;
      break;
    }
  }

  // Commit if necessary.
  while (true) {
    usize committed = committed_size_.load(std::memory_order_acquire);

    if (new_size <= committed) {
      break;
    }

    usize new_committed = base::round_up(new_size, kPageSize);
    if (new_committed > capacity_) [[unlikely]] {
      new_committed = capacity_;
    }

    if (committed_size_.compare_exchange_weak(committed, new_committed,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
      const usize diff = new_committed - committed;
      char* commit_ptr = ptr_ + committed;

      if (!commit_pages(commit_ptr, diff)) [[unlikely]] {
        dcheck_msg(false, "Failed to commit pages for arena.");
        return nullptr;
      }
      break;
    }
    // Retry
  }

  return ptr_ + old_size;
}

}  // namespace mem
