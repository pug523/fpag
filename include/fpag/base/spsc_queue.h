// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>

#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/mem/cache.h"
#include "fpag/mem/page_allocator.h"

namespace base {

// SPSC (Single-Producer Single-Consumer) Queue
class SpscQueue {
 public:
  enum class Mode : u8 {
    Drop,
    Block,

    Default = Drop,
  };

  enum class DequeueStatus : u8 {
    Ok,
    Empty,
  };

  enum class EnqueueStatus : u8 {
    Ok,
    Dropped,
  };

  SpscQueue() = default;
  ~SpscQueue() { reset(); }

  SpscQueue(const SpscQueue&) = delete;
  SpscQueue& operator=(const SpscQueue&) = delete;

  SpscQueue(SpscQueue&&) noexcept;
  SpscQueue& operator=(SpscQueue&&) noexcept;

  // Initialize the queue with the given data buffer and capacity.
  // `capacity` must be a power of 2.
  void init(usize capacity = default_capacity(), Mode mode = Mode::Default);
  void reset();

  // Zero-copied consumer interface
  const char* peek(usize size, usize align = 1);
  void discard(usize size, usize align = 1);

  // Copied dequeue (wrapper of peek/discard)
  DequeueStatus dequeue(void* dest, usize size, usize align = 1);

  // Zero-copied producer interface
  EnqueueStatus reserve(usize size, void** out, usize align = 1);
  void commit(usize size);

  // Copied enqueue (wrapper of reserve/commit)
  EnqueueStatus enqueue(const void* new_data, usize size, usize align = 1);

  usize capacity() const { return capacity_; }
  usize size() const {
    return tail_.load(std::memory_order_relaxed) -
           head_.load(std::memory_order_relaxed);
  }
  bool empty() const { return size() == 0; }
  usize available() const { return capacity_ - size(); }

  usize size_consumer() const {
    return tail_.load(std::memory_order_relaxed) - head_cache_;
  }
  usize size_producer() const {
    return tail_cache_ - head_.load(std::memory_order_relaxed);
  }

  const char* head_ptr() const {
    return data_ + (head_.load(std::memory_order_acquire) & capacity_mask());
  }

  char* tail_ptr() {
    return data_ + (tail_.load(std::memory_order_acquire) & capacity_mask());
  }

  usize head_cache() const { return head_cache_; }
  usize tail_cache() const { return tail_cache_; }

  usize dropped_count() const { return dropped_count_; }
  usize blocked_count() const { return blocked_count_; }

  static usize default_capacity() {
    return base::next_power_of_two(mem::page_size());
  }
  static constexpr usize kMaxCapacity = static_cast<usize>(1) << 35;  // 32 GiB

 private:
  usize capacity_mask() const { return capacity_ - 1; }

  usize available_producer() const;

  void wait_for_space_producer(usize size) const;

  char* data_ = nullptr;
  usize capacity_ = 0;
  Mode mode_ = Mode::Default;

  usize dropped_count_ = 0;
  usize blocked_count_ = 0;

  // Consumer
  alignas(mem::kCacheLineSize) std::atomic<usize> head_ = 0;
  alignas(mem::kCacheLineSize) usize head_cache_ = 0;

  // Producer
  alignas(mem::kCacheLineSize) std::atomic<usize> tail_ = 0;
  alignas(mem::kCacheLineSize) usize tail_cache_ = 0;
};

}  // namespace base
