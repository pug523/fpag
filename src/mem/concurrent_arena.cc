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

namespace {

// How far ahead of the frontier the pages are made writable. See
// ConcurrentArena::alloc.
constexpr usize COMMIT_CHUNK_BYTES = 256ull << 10;

}  // namespace

ConcurrentArena::ConcurrentArena(ConcurrentArena&& other) noexcept
    : ptr_(std::exchange(other.ptr_, nullptr)),
      capacity_(std::exchange(other.capacity_, 0)),
      size_(other.size_.load(std::memory_order_relaxed)),
      committed_size_(other.committed_size_.load(std::memory_order_relaxed)) {
  other.size_.store(0, std::memory_order_relaxed);
  other.committed_size_.store(0, std::memory_order_relaxed);
}

ConcurrentArena& ConcurrentArena::operator=(ConcurrentArena&& other) noexcept {
  if (this != &other) [[likely]] {
    if (ptr_) {
      reset();
    }

    ptr_ = std::exchange(other.ptr_, nullptr);
    capacity_ = std::exchange(other.capacity_, 0);

    size_.store(other.size_.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
    committed_size_.store(other.committed_size_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);

    other.size_.store(0, std::memory_order_relaxed);
    other.committed_size_.store(0, std::memory_order_relaxed);
  }

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

// Makes the first `end` bytes writable, answering false when a page cannot be.
//
// The pages become writable before the watermark that advertises them is
// published. Publishing first would let a thread whose allocation lands inside
// the advertised range return a pointer into a region that is still PROT_NONE,
// because the mprotect of the publishing thread had not run yet. Committing
// first costs a repeated, idempotent mprotect when two threads race, and makes
// `end <= committed` a guarantee that the pages are already writable.
//
// The watermark advances a chunk at a time rather than to the byte that needed
// it. Committing exactly what was asked for makes every allocation that lands
// on a fresh page a system call, and several threads allocating at once cross
// the frontier over and over, so a run spends more time in the kernel than in
// the work. A chunk costs the same system call for a quarter of a thousand
// allocations, and a committed page that nothing writes is not resident, so
// the reservation stays as lazy as it was.
bool ConcurrentArena::commit_until(usize end) {
  while (true) {
    usize committed = committed_size_.load(std::memory_order_acquire);
    if (end <= committed) {
      return true;
    }

    usize next = base::round_up(end, COMMIT_CHUNK_BYTES);
    if (next > capacity_) [[unlikely]] {
      next = capacity_;
    }

    if (!commit_pages(ptr_ + committed, next - committed)) [[unlikely]] {
      FPAG_DCHECK_MSG(false, "Failed to commit pages for arena.");
      return false;
    }

    if (committed_size_.compare_exchange_weak(committed, next,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
      return true;
    }
  }
}

void ConcurrentArena::set_lanes(usize lanes, usize align) {
  FPAG_DCHECK_MSG(size_.load(std::memory_order_relaxed) == 0,
                  "Arena has allocated already; a lane is a slice of room the "
                  "offset has not reached yet.");
  FPAG_DCHECK_MSG(lanes > 0, "An arena has at least one lane.");
  FPAG_DCHECK_MSG(align > 0, "A lane holds something.");

  // The slices cover the reservation in lane order, and the claim slack is
  // held back from the end of it, because it is the room a check leaves for
  // the takes racing with it and a lane is where those takes are. Both ends
  // are rounded to the lane's alignment so that what a lane produced is the
  // run from its begin to its cursor and nothing else.
  const usize usable = base::round_down(capacity_ - claim_slack_, align);
  lanes_.resize(lanes);
  for (usize i = 0; i < lanes; ++i) {
    const usize begin = base::round_up((usable * i) / lanes, align);
    const usize end = base::round_down((usable * (i + 1)) / lanes, align);
    lanes_[i].cursor = begin;
    lanes_[i].begin = begin;
    lanes_[i].end = end > begin ? end : begin;
  }
}

void* ConcurrentArena::alloc_from(usize lane, usize size, usize align) {
  FPAG_DCHECK(ptr_);
  FPAG_DCHECK_MSG(lane < lanes_.size(), "No such lane.");
  FPAG_DCHECK_MSG(align != 0 && size % align == 0,
                  "alloc_from needs a size that is a whole number of the "
                  "alignment its lane was set with.");
  FPAG_DCHECK_MSG(claim_slack_ >= size,
                  "alloc_from needs claim slack for the takes that race with "
                  "it; see set_claim_slack.");

  Lane& cursor = lanes_[lane];
  // One writer, so reading the cursor, checking it and moving it are the only
  // operations. Nothing has to be aligned here: the slice's ends were rounded
  // when the lane was set, and every request from a lane is a whole number of
  // that alignment, so the cursor is aligned where it starts and stays so.
  const usize at = cursor.cursor;
  FPAG_DCHECK_EQ(at % align, 0);
  if (size > cursor.end - at) [[unlikely]] {
    return nullptr;
  }
  cursor.cursor = at + size;

  if (!commit_until(at + size)) [[unlikely]] {
    FPAG_DCHECK_MSG(false, "Failed to commit pages for arena.");
    return nullptr;
  }
  return ptr_ + at;
}

usize ConcurrentArena::lane_size(usize lane) const {
  if (lanes_.empty()) {
    FPAG_DCHECK_EQ(lane, 0u);
    return size_.load(std::memory_order_relaxed);
  }
  FPAG_DCHECK_MSG(lane < lanes_.size(), "No such lane.");
  return lanes_[lane].cursor - lanes_[lane].begin;
}

usize ConcurrentArena::lane_begin(usize lane) const {
  if (lanes_.empty()) {
    FPAG_DCHECK_EQ(lane, 0u);
    return 0;
  }
  FPAG_DCHECK_MSG(lane < lanes_.size(), "No such lane.");
  return lanes_[lane].begin;
}

usize ConcurrentArena::lane_end(usize lane) const {
  if (lanes_.empty()) {
    FPAG_DCHECK_EQ(lane, 0u);
    return size_.load(std::memory_order_relaxed);
  }
  FPAG_DCHECK_MSG(lane < lanes_.size(), "No such lane.");
  return lanes_[lane].end;
}

void ConcurrentArena::set_claim_slack(usize bytes) {
  FPAG_DCHECK_MSG(size_.load(std::memory_order_relaxed) == 0,
                  "Arena has allocated already; the slack it holds back is "
                  "room the offset has not reached yet.");
  FPAG_DCHECK_MSG(lanes_.empty(),
                  "The lanes are slices of the room the slack holds back; set "
                  "this before set_lanes.");
  claim_slack_ = bytes;
}

void* ConcurrentArena::alloc_exact(usize size, usize align) {
  FPAG_DCHECK(ptr_);
  FPAG_DCHECK_MSG(align != 0 && size % align == 0,
                  "alloc_exact needs a size that is a whole number of its "
                  "alignment.");

  // Taking the offset with one operation is what this is for: it cannot fail
  // and it cannot need a second look. Whether there is room is the caller's to
  // have checked, and it checks for the takes that race with it, so the offset
  // this leaves is the offset every take before it left.
  const usize at = size_.fetch_add(size, std::memory_order_acq_rel);
  // Every allocation from this arena has been the same size, which its
  // alignment divides, so the offset is where this call left it.
  FPAG_DCHECK_EQ(at % align, 0);
  FPAG_DCHECK_MSG(size <= capacity_ - at,
                  "alloc_exact was called without room for its request; the "
                  "caller checks the reservation before it takes from it.");
  if (!commit_until(at + size)) [[unlikely]] {
    // A page that cannot be made writable is not a request that does not fit:
    // there is no answer that the caller could act on, and the offset has
    // already moved.
    FPAG_DCHECK_MSG(false, "Failed to commit pages for arena.");
    return nullptr;
  }
  return ptr_ + at;
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

  if (!commit_until(new_size)) [[unlikely]] {
    return nullptr;
  }

  return ptr_ + old_size;
}

}  // namespace mem
