// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstring>
#include <utility>

#include "base/debug/check.h"
#include "base/mem/arena_allocator.h"
#include "base/numeric.h"

namespace base {

template <typename T, usize kMaxChunks = 256>
class BlockedArenaArray {
 public:
  static constexpr usize kElementsPerChunk =
      (ArenaAllocator::kBlockSize - sizeof(void*)) / sizeof(T);
  static_assert(kElementsPerChunk > 0);

  explicit BlockedArenaArray(bool use_huge_pages = true)
      : use_huge_pages_(use_huge_pages) {
    // Allocate the block pointer table once; zero-initialize.
    blocks_ = static_cast<T**>(
        arena_.alloc(sizeof(T*) * kMaxChunks, use_huge_pages_, alignof(T*)));
    std::memset(blocks_, 0, sizeof(T*) * kMaxChunks);
    block_count_.store(0, std::memory_order_relaxed);
    total_size_.store(0, std::memory_order_relaxed);
  }

  ~BlockedArenaArray() = default;

  BlockedArenaArray(const BlockedArenaArray&) = delete;
  BlockedArenaArray& operator=(const BlockedArenaArray&) = delete;

  BlockedArenaArray(BlockedArenaArray&&) noexcept = delete;
  BlockedArenaArray& operator=(BlockedArenaArray&&) noexcept = delete;

  // Emplace a new element and return pointer to it.
  template <typename... Args>
  T* emplace_back(Args&&... args) {
    // Reserve a slot index atomically.
    const usize index = total_size_.fetch_add(1, std::memory_order_acq_rel);
    const usize block_idx = index / kElementsPerChunk;
    const usize offset = index % kElementsPerChunk;

    // Ensure required block exists. If not, allocate it while holding the lock.
    ensure_block(block_idx);

    // Construct object in-place.
    T* storage = &blocks_[block_idx][offset];
    T* obj = new (storage) T(std::forward<Args>(args)...);
    return obj;
  }

  [[nodiscard]] inline T& operator[](usize index) {
    dcheck_lt(index, total_size_.load(std::memory_order_acquire));
    const usize block_idx = index / kElementsPerChunk;
    const usize offset = index % kElementsPerChunk;
    return blocks_[block_idx][offset];
  }

  [[nodiscard]] inline const T& operator[](usize index) const {
    dcheck_lt(index, total_size_.load(std::memory_order_acquire));
    const usize block_idx = index / kElementsPerChunk;
    const usize offset = index % kElementsPerChunk;
    return blocks_[block_idx][offset];
  }

  [[nodiscard]] inline usize size() const {
    return total_size_.load(std::memory_order_acquire);
  }

 private:
  void ensure_block(usize required_block_idx) {
    // Chunk already allocated.
    if (required_block_idx < block_count_.load(std::memory_order_acquire)) {
      return;
    }

    // Acquire spinlock to allocate missing blocks safely. Only one thread will
    // perform allocation for a given block index.
    while (lock_.test_and_set(std::memory_order_acquire)) {
      // busy-wait; a platform-specific pause could be added here
    }

    // Re-check after acquiring lock in case another thread allocated it.
    usize current_blocks = block_count_.load(std::memory_order_relaxed);
    while (current_blocks <= required_block_idx) {
      dcheck_lt_msg(current_blocks, kMaxChunks, "exceeded max blocks");

      T* new_storage = static_cast<T*>(arena_.alloc(
          sizeof(T) * kElementsPerChunk, use_huge_pages_, alignof(T)));
      blocks_[current_blocks] = new_storage;
      ++current_blocks;
      // Continue until required_block_idx is satisfied.
    }

    // Publish new block_count_.
    block_count_.store(current_blocks, std::memory_order_release);

    // Release lock.
    lock_.clear(std::memory_order_release);
  }

  T** blocks_ = nullptr;
  std::atomic<usize> block_count_{0};
  ArenaAllocator arena_;
  std::atomic<usize> total_size_{0};
  bool use_huge_pages_ = false;

  // Simple spinlock for block allocation.
  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};

}  // namespace base
