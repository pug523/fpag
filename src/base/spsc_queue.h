// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>

#include "base/numeric.h"
#include "mem/cache.h"

namespace base {

// SPSC (Single-Producer Single-Consumer) Queue
class SpscQueue {
 public:
  enum class Mode : u8 {
    kDrop,
    kBlock,

    kDefault = kDrop,
  };
  enum class DequeueStatus : u8 {
    kOk,
    kEmpty,
  };
  enum class EnqueueStatus : u8 {
    kOk,
    kDropped,
  };

  SpscQueue() = default;
  ~SpscQueue() { reset(); }

  SpscQueue(const SpscQueue&) = delete;
  SpscQueue& operator=(const SpscQueue&) = delete;

  SpscQueue(SpscQueue&&) noexcept;
  SpscQueue& operator=(SpscQueue&&) noexcept;

  // Initialize the queue with the given data buffer and capacity.
  // `capacity` must be a power of 2.
  void init(usize capacity = kDefaultCapacity, Mode mode = Mode::kDefault);
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

  inline usize capacity() const { return capacity_; }
  inline usize size() const {
    return tail_.load(std::memory_order_relaxed) -
           head_.load(std::memory_order_relaxed);
  }
  inline bool empty() const { return size() == 0; }
  inline usize available() const { return capacity_ - size(); }

  inline usize size_consumer() const {
    return tail_.load(std::memory_order_relaxed) - head_cache_;
  }
  inline usize size_producer() const {
    return tail_cache_ - head_.load(std::memory_order_relaxed);
  }

  inline const char* head_ptr() const {
    return data_ + (head_.load(std::memory_order_acquire) & capacity_mask());
  }

  inline char* tail_ptr() {
    return data_ + (tail_.load(std::memory_order_acquire) & capacity_mask());
  }

  static constexpr usize kDefaultCapacity = 1 << 12;                  // 4 KiB
  static constexpr usize kMaxCapacity = static_cast<usize>(1) << 35;  // 32 GiB

 private:
  inline usize capacity_mask() const { return capacity_ - 1; }

  inline usize available_producer() const;

  void wait_for_space_producer(usize size) const;

  char* data_ = nullptr;
  usize capacity_ = 0;
  Mode mode_ = Mode::kDefault;

  // Consumer
  alignas(mem::kCacheLineSize) std::atomic<usize> head_ = 0;
  alignas(mem::kCacheLineSize) usize head_cache_ = 0;

  // Producer
  alignas(mem::kCacheLineSize) std::atomic<usize> tail_ = 0;
  alignas(mem::kCacheLineSize) usize tail_cache_ = 0;
};

}  // namespace base
