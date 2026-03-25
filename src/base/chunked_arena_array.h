// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstring>
#include <utility>

#include "base/arena_allocator.h"

namespace base {

template <typename T, usize kMaxChunks = 256>
class ChunkedArenaArray {
 public:
  static constexpr usize kElementsPerChunk =
      (ArenaAllocator::kChunkSize - sizeof(void*)) / sizeof(T);
  static_assert(kElementsPerChunk > 0);

  explicit ChunkedArenaArray(bool use_huge_pages = true)
      : use_huge_pages_(use_huge_pages) {
    // Allocate the chunk pointer table once; zero-initialize.
    chunks_ = static_cast<T**>(
        arena_.alloc(sizeof(T*) * kMaxChunks, use_huge_pages_, alignof(T*)));
    std::memset(chunks_, 0, sizeof(T*) * kMaxChunks);
    chunk_count_.store(0, std::memory_order_relaxed);
    total_size_.store(0, std::memory_order_relaxed);
  }

  ~ChunkedArenaArray() = default;

  ChunkedArenaArray(const ChunkedArenaArray&) = delete;
  ChunkedArenaArray& operator=(const ChunkedArenaArray&) = delete;

  ChunkedArenaArray(ChunkedArenaArray&&) noexcept = delete;
  ChunkedArenaArray& operator=(ChunkedArenaArray&&) noexcept = delete;

  // Emplace a new element and return pointer to it.
  template <typename... Args>
  T* emplace_back(Args&&... args) {
    // Reserve a slot index atomically.
    const usize index = total_size_.fetch_add(1, std::memory_order_acq_rel);
    const usize chunk_idx = index / kElementsPerChunk;
    const usize offset = index % kElementsPerChunk;

    // Ensure required chunk exists. If not, allocate it while holding the lock.
    ensure_chunk(chunk_idx);

    // Construct object in-place.
    T* storage = &chunks_[chunk_idx][offset];
    T* obj = new (storage) T(std::forward<Args>(args)...);
    return obj;
  }

  [[nodiscard]] inline T& operator[](usize index) {
    dcheck(index < total_size_.load(std::memory_order_acquire));
    const usize chunk_idx = index / kElementsPerChunk;
    const usize offset = index % kElementsPerChunk;
    return chunks_[chunk_idx][offset];
  }

  [[nodiscard]] inline const T& operator[](usize index) const {
    dcheck(index < total_size_.load(std::memory_order_acquire));
    const usize chunk_idx = index / kElementsPerChunk;
    const usize offset = index % kElementsPerChunk;
    return chunks_[chunk_idx][offset];
  }

  [[nodiscard]] inline usize size() const {
    return total_size_.load(std::memory_order_acquire);
  }

 private:
  void ensure_chunk(usize required_chunk_idx) {
    // Chunk already allocated.
    if (required_chunk_idx < chunk_count_.load(std::memory_order_acquire)) {
      return;
    }

    // Acquire spinlock to allocate missing chunks safely. Only one thread will
    // perform allocation for a given chunk index.
    while (lock_.test_and_set(std::memory_order_acquire)) {
      // busy-wait; a platform-specific pause could be added here
    }

    // Re-check after acquiring lock in case another thread allocated it.
    usize current_chunks = chunk_count_.load(std::memory_order_relaxed);
    while (current_chunks <= required_chunk_idx) {
      dcheck_msg(current_chunks < kMaxChunks, "exceeded max chunks");

      T* new_storage = static_cast<T*>(arena_.alloc(
          sizeof(T) * kElementsPerChunk, use_huge_pages_, alignof(T)));
      chunks_[current_chunks] = new_storage;
      ++current_chunks;
      // Continue until required_chunk_idx is satisfied.
    }

    // Publish new chunk_count_.
    chunk_count_.store(current_chunks, std::memory_order_release);

    // Release lock.
    lock_.clear(std::memory_order_release);
  }

  T** chunks_ = nullptr;
  std::atomic<usize> chunk_count_{0};
  ArenaAllocator arena_;
  std::atomic<usize> total_size_{0};
  bool use_huge_pages_ = false;

  // Simple spinlock for chunk allocation.
  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};

}  // namespace base
