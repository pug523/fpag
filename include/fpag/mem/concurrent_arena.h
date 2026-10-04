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

  // Reserves `bytes` of the reservation that no allocation may hand out.
  // Several threads allocating at once each check the room and then take it,
  // and the two are not one step, so a check has to leave room for the takes
  // that follow it. A caller that takes a slot per worker says so here.
  //
  // Zero, the default, is exact for one caller: the room it may hand out ends
  // at the end of the reservation. It is only a caller with threads that needs
  // more, because a take that finds no room has already moved the offset, and
  // the caller that raced with it would then find room that is not there.
  // Not thread-safe, and it must be set before the first allocation, because
  // what it holds back is room the offset has not reached yet.
  void set_claim_slack(usize bytes);

  // Lock-free allocation.
  [[nodiscard]] void* alloc(usize size,
                            usize align = alignof(std::max_align_t));

  // Divides the reservation into `lanes` cursors, each with a slice of its own.
  // Every cursor may be advanced by one thread and by no other, so a lane is
  // the answer for a caller whose threads each want to allocate: a bump that
  // nobody else can move needs no atomic operation at all.
  //
  // `align` is what a lane holds. A slice begins where the last one ended, and
  // a caller reads what a lane produced as the run from its begin to its
  // cursor, so both ends are rounded to `align`: the fill between slices is
  // then not counted as something a lane produced, and the offsets a lane
  // hands out are aligned without anything having to align them.
  //
  // The slices are in lane order and cover the reservation, so the offset a
  // lane hands out is still an offset into the whole arena, and a caller that
  // addresses its objects by offset -- a table whose index is `offset / size`
  // -- needs no change to how it reads them. What a lane costs is the part of
  // its slice it does not fill, and what it cannot do is borrow: a lane that
  // reaches its end answers nullptr even when another lane has room, so a
  // caller that may be handed uneven work keeps alloc as the answer for that
  // case.
  //
  // Not thread-safe, and it must be set before the first allocation.
  void set_lanes(usize lanes, usize align);

  // Allocates `size` bytes from one lane, which must be a whole number of the
  // alignment the lane was set with. A lane has one writer, so this reads the
  // cursor, checks it and moves it and does nothing else. It answers nullptr
  // when the lane is full, having written nothing, so the runs a caller counts
  // are the runs it handed out.
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
  // reservation can hold this request and the takes that can race with it,
  // the way it does for alloc: it reads the offset, leaves a slot in hand for
  // every take in flight, and stops once the last slot would not fit. This
  // takes the room that check left. A take that finds none is a caller that
  // did not check, so it is a check failure rather than an answer -- and that
  // is what keeps the offset exact. `alloc` answers nullptr and leaves the
  // offset where the refused request would have put it, so a table that counts
  // its nodes by the offset counts one that is not there and walks past its
  // end.
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
  // Room held back for the claims that race with a check. See set_claim_slack.
  usize claim_slack_ = 0;

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
  std::vector<Lane> lanes_;

  std::atomic<usize> size_{0};
  std::atomic<usize> committed_size_{0};
};

}  // namespace mem
