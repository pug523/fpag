// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstddef>
#include <utility>
#include <vector>

#include "fpag/base/numeric.h"
#include "fpag/debug/check.h"
#include "fpag/mem/arena_ptr.h"

namespace mem {

class ConcurrentArena {
 public:
  ConcurrentArena() = default;
  ~ConcurrentArena() {
    if (ptr_) {
      reset();
    }
  }

  ConcurrentArena(const ConcurrentArena&) = delete;
  ConcurrentArena& operator=(const ConcurrentArena&) = delete;

  ConcurrentArena(ConcurrentArena&& other) noexcept;
  ConcurrentArena& operator=(ConcurrentArena&& other) noexcept;

  // Not thread-safe (must be called before use).
  void reserve(usize capacity);

  // Not thread-safe.
  void reset();

  // Lock-free allocation.
  [[nodiscard]] void* alloc(usize size,
                            usize align = alignof(std::max_align_t));

  // Divides the reservation into `lanes` cursors, each with a slice of its own.
  // Every cursor may be advanced by one thread and by no other, so a lane is
  // the answer for a caller whose threads each want to allocate: a bump that
  // nobody else can move needs no atomic operation at all.
  //
  // `unit` is the size of what a lane holds, and both ends of a slice are
  // whole numbers of it. That is the caller's whole reason to ask: a table
  // whose index is `offset / unit` cannot have a slice that begins part-way
  // into an object, because the division would truncate and every object after
  // it would be read from the one before. A unit is not an alignment, so it
  // need not be a power of two.
  //
  // The slices are in lane order below the pool, so the offset a lane hands
  // out is still an offset into the whole arena, and a caller that addresses
  // its objects by offset needs no change to how it reads them.
  //
  // A lane cannot borrow: it answers nullptr when it reaches its end, even
  // when another lane has room. `pool_bytes` is the room that answers for
  // that, and it is the whole reason this takes three numbers rather than two.
  // The appends of a caller that spreads work over threads are not spread the
  // way its room is, so dividing the room by the lane count and stopping there
  // means a package that fits a table with room to spare is refused once the
  // lanes are uneven. The pool is at the end of the reservation, whole units,
  // and the caller reaches it through alloc and alloc_exact as it did before
  // there were lanes. `pool_bytes` of zero is a caller that has balanced its
  // work and wants no room for the case where it has not.
  //
  // Not thread-safe, and it must be set before the first allocation.
  void set_lanes(usize lanes, usize unit, usize pool_bytes);

  // Allocates `size` bytes from one lane, which must be a whole number of the
  // unit the lane was set with, and aligned to `align`. A lane has one writer,
  // so this reads the cursor, checks it and moves it and does nothing else. It
  // answers nullptr when the lane is full, having written nothing, so the runs
  // a caller counts are the runs it handed out.
  [[nodiscard]] void* alloc_from(usize lane, usize size, usize align);

  // How much a lane has handed out, which is what a caller walks to. Before
  // the lane is set, there is one lane and this is size().
  [[nodiscard]] usize lane_size(usize lane) const;

  // The run a lane produced: where its first allocation went, and how many
  // bytes it handed out. A caller walks [lane_begin, lane_begin + lane_size)
  // for what the lane holds, where size() is the single number it read before
  // there were lanes.
  [[nodiscard]] usize lane_begin(usize lane) const;
  [[nodiscard]] usize lane_end(usize lane) const;

  // The run the shared cursor hands out, which is the whole of what the arena
  // has used when there are no lanes. A caller that reads a table by index
  // counts this among the runs that hold what it wrote: a lane that reached
  // its end falls back here, so the room a table has filled is the lane runs
  // and this one together.
  [[nodiscard]] usize pool_begin() const { return pool_begin_; }
  [[nodiscard]] usize pool_size() const {
    return size_.load(std::memory_order_relaxed) - pool_begin_;
  }

  // Allocates `size` bytes in one operation, for a table that appends one size
  // of object and no other.
  //
  // Two things are the caller's side of the bargain. The first is shape:
  // `size` is a whole number of `align`, and every allocation from this arena
  // has been the same size, which holds for a table of one node type. The
  // offset is then a whole number of nodes, and the alignment of a node
  // divides its size, so the next offset is aligned whenever this one is.
  //
  // The second is room. The caller has already established that the
  // reservation holds the request, the way it does for alloc: it reads the
  // offset and stops once the request would not fit. A take that finds none is
  // a caller that did not check, so it is a check failure rather than an
  // answer -- and that is what keeps the offset exact. `alloc` answers nullptr
  // and leaves the offset where the refused request would have put it, so a
  // table that counts its nodes by the offset counts one that is not there and
  // walks past its end. A caller with several appenders gives each of them a
  // lane instead, which is what set_lanes is for.
  //
  // What this is for: `alloc` reads the offset, aligns it, and compares and
  // swaps, which under contention costs several transfers of the line the
  // offset lives on for every node in the package. This skips the read and the
  // retry and takes the offset once.
  [[nodiscard]] void* alloc_exact(usize size, usize align);

  // Returns nullptr, without constructing anything, if the arena is full.
  template <typename T, typename... Args>
  [[nodiscard]] inline T* create(Args&&... args) {
    void* mem = alloc(sizeof(T), alignof(T));
    if (mem == nullptr) [[unlikely]] {
      return nullptr;
    }
    return new (mem) T(std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  [[nodiscard]] inline ArenaUniquePtr<T> create_managed(Args&&... args) {
    return ArenaUniquePtr<T>(create<T>(std::forward<Args>(args)...));
  }

  inline const char* base_ptr() const { return ptr_; }
  inline usize capacity() const { return capacity_; }
  inline usize size() const { return size_.load(std::memory_order_relaxed); }
  inline usize committed_size() const {
    return committed_size_.load(std::memory_order_relaxed);
  }

 private:
  // Makes the first `end` bytes writable. See alloc and alloc_exact.
  [[nodiscard]] bool commit_until(usize end);

  char* ptr_ = nullptr;
  usize capacity_ = 0;
  // The size of what a lane holds, which its slice ends are whole numbers of.
  usize lane_unit_ = 1;
  // Where the pool begins, which is the end of the last lane. Zero when there
  // are no lanes, so that the pool is what the shared cursor reaches.
  usize pool_begin_ = 0;

  // One cursor per lane, empty until set_lanes names them: an arena without
  // lanes is one lane, and size() is what that lane has handed out. The cursor
  // is not atomic because a lane has one writer, and the reader of lane_size is
  // either that writer or a caller the writer has finished before -- which is
  // the same promise that makes the walk over a lane a walk and not a race.
  struct Lane {
    usize cursor = 0;
    usize begin = 0;
    usize end = 0;
  };
  // Declared before the offset atomics so that a move takes the lanes and the
  // reservation together, and so that reset() clears them with the pages.
  std::vector<Lane> lanes_;

  std::atomic<usize> size_{0};
  std::atomic<usize> committed_size_{0};
};

}  // namespace mem
