// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/mem/concurrent_arena.h"

#include <atomic>
#include <utility>

#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/debug/check.h"
#include "fpag/mem/page_allocator.h"

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
  FPAG_DCHECK_MSG(!ptr_, "Arena is already reserved.");

  capacity_ = capacity;
  FPAG_DCHECK_MSG(is_page_aligned_size(capacity_),
                  "Capacity must be page aligned.");

  ptr_ = static_cast<char*>(reserve_pages(capacity_));
  FPAG_DCHECK_MSG(ptr_, "Failed to reserve pages for arena.");

  size_.store(0, std::memory_order_relaxed);
  committed_size_.store(0, std::memory_order_relaxed);
}

void ConcurrentArena::reset() {
  FPAG_DCHECK(ptr_);

  free_pages(ptr_, capacity_);
  ptr_ = nullptr;
  capacity_ = 0;

  size_.store(0, std::memory_order_relaxed);
  committed_size_.store(0, std::memory_order_relaxed);
}

void* ConcurrentArena::alloc(usize size, usize align) {
  FPAG_DCHECK(ptr_);

  usize old_size = 0;
  usize new_size = 0;

  // Bump pointer
  while (true) {
    old_size = size_.load(std::memory_order_relaxed);
    const usize aligned = base::round_up(old_size, align);
    // Prevent wrap-around before comparing with capacity.
    if (aligned > capacity_ || size > capacity_ - aligned) [[unlikely]] {
      FPAG_DCHECK_MSG(false, "Arena capacity exceeded.");
      return nullptr;
    }
    new_size = aligned + size;

    if (size_.compare_exchange_weak(old_size, new_size,
                                    std::memory_order_acq_rel,
                                    std::memory_order_relaxed)) {
      // Success
      old_size = aligned;
      break;
    }
  }

  // Commit if necessary.
  //
  // The pages become writable before the watermark that advertises them is
  // published. Publishing first would let a thread whose allocation lands
  // inside the advertised range return a pointer into a region that is still
  // PROT_NONE, because the mprotect of the publishing thread had not run yet.
  // Committing first costs a repeated, idempotent mprotect when two threads
  // race, and makes `new_size <= committed` a guarantee that the pages are
  // already committed.
  while (true) {
    usize committed = committed_size_.load(std::memory_order_acquire);

    if (new_size <= committed) {
      break;
    }

    usize new_committed = base::round_up(new_size, page_size());
    if (new_committed > capacity_) [[unlikely]] {
      new_committed = capacity_;
    }

    const usize diff = new_committed - committed;
    if (!commit_pages(ptr_ + committed, diff)) [[unlikely]] {
      FPAG_DCHECK_MSG(false, "Failed to commit pages for arena.");
      return nullptr;
    }

    if (committed_size_.compare_exchange_weak(committed, new_committed,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
      break;
    }
  }

  return ptr_ + old_size;
}

}  // namespace mem
