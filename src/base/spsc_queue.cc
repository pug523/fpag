// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/spsc_queue.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>

#include "base/debug/check.h"
#include "base/math_util.h"
#include "base/numeric.h"
#include "build/build_config.h"
#include "mem/page_allocator.h"

namespace base {

SpscQueue::SpscQueue(SpscQueue&& other) noexcept {
  data_ = other.data_;
  capacity_ = other.capacity_;
  mode_ = other.mode_;
  head_ = other.head_.load(std::memory_order_relaxed);
  head_cache_ = other.head_cache_;
  tail_ = other.tail_.load(std::memory_order_relaxed);
  tail_cache_ = other.tail_cache_;
  other.data_ = nullptr;
  other.capacity_ = 0;
  other.mode_ = Mode::kDefault;
  other.head_.store(0, std::memory_order_relaxed);
  other.head_cache_ = 0;
  other.tail_.store(0, std::memory_order_relaxed);
  other.tail_cache_ = 0;
}

SpscQueue& SpscQueue::operator=(SpscQueue&& other) noexcept {
  data_ = other.data_;
  capacity_ = other.capacity_;
  mode_ = other.mode_;
  head_ = other.head_.load(std::memory_order_relaxed);
  head_cache_ = other.head_cache_;
  tail_ = other.tail_.load(std::memory_order_relaxed);
  tail_cache_ = other.tail_cache_;
  other.data_ = nullptr;
  other.capacity_ = 0;
  other.mode_ = Mode::kDefault;
  other.head_.store(0, std::memory_order_relaxed);
  other.head_cache_ = 0;
  other.tail_.store(0, std::memory_order_relaxed);
  other.tail_cache_ = 0;

  return *this;
}

void SpscQueue::init(usize capacity, Mode mode) {
  capacity_ = capacity;
  FPAG_DCHECK_MSG(base::is_power_of_two(capacity_),
                  "SpscQueue: capacity must be a power of two.");
  FPAG_DCHECK_MSG(capacity_ <= kMaxCapacity,
                  "SpscQueue: capacity must be <= kMaxCapacity");

  mode_ = mode;

  data_ = static_cast<char*>(mem::allocate_aliased_pages(capacity_));
  FPAG_DCHECK_MSG(data_, "SpscQueue: failed to allocate memory for queue data");
  FPAG_DCHECK(reinterpret_cast<uintptr_t>(data_) % 8 == 0);
}

void SpscQueue::reset() {
  if (data_) {
    mem::free_pages(data_, capacity_);
    data_ = nullptr;
  }
  capacity_ = 0;
  mode_ = Mode::kDefault;
  head_.store(0, std::memory_order_relaxed);
  head_cache_ = 0;
  tail_.store(0, std::memory_order_relaxed);
  tail_cache_ = 0;
}

const char* SpscQueue::peek(usize size, usize align) {
  FPAG_DCHECK_GE(size_consumer(), size);
  const usize aligned_head = base::round_up(head_cache_, align);
  FPAG_DCHECK_GE(size_consumer(), size + aligned_head - head_cache_);
  // return head_ptr();  // `data_` is double buffered; safe
  return data_ +
         (aligned_head & capacity_mask());  // `data_` is double buffered; safe
}

void SpscQueue::discard(usize size, usize align) {
  usize aligned_head = base::round_up(head_cache_, align);
  head_cache_ = aligned_head + size;
  head_.store(head_cache_, std::memory_order_release);
}

SpscQueue::DequeueStatus SpscQueue::dequeue(void* dest,
                                            usize size,
                                            usize align) {
  FPAG_DCHECK_LE(size, capacity_);

  if (size_consumer() < size) {
    return DequeueStatus::kEmpty;
  }

  const char* src = peek(size, align);
  std::memcpy(dest, src, size);

  discard(size, align);
  return DequeueStatus::kOk;
}

SpscQueue::EnqueueStatus SpscQueue::reserve(usize size,
                                            void** out,
                                            usize align) {
  FPAG_DCHECK_LE(size, capacity_);

  const usize current_tail = tail_cache_;
  const usize aligned_tail = base::round_up(current_tail, align);
  const usize padding = aligned_tail - current_tail;
  const usize total_needed = size + padding;

  if (available_producer() < size) [[unlikely]] {
    if (mode_ == Mode::kDrop) {
      return EnqueueStatus::kDropped;
    }
    wait_for_space_producer(total_needed);
  }

  // Advance only `tail_cache_` to `aligned_tail`, not for `tail_`.
  tail_cache_ = aligned_tail;
  *out = static_cast<void*>(data_ + (aligned_tail & capacity_mask()));

  FPAG_DCHECK(reinterpret_cast<uintptr_t>(*out) % align == 0);
  return EnqueueStatus::kOk;
}

void SpscQueue::commit(usize size) {
  tail_cache_ += size;
  tail_.store(tail_cache_, std::memory_order_release);
}

SpscQueue::EnqueueStatus SpscQueue::enqueue(const void* new_data,
                                            usize size,
                                            usize align) {
  FPAG_DCHECK_LE(size, capacity_);
  void* buf = nullptr;
  EnqueueStatus status = reserve(size, &buf, align);
  if (status != EnqueueStatus::kOk) [[unlikely]] {
    return status;
  }

  std::memcpy(buf, new_data, size);
  commit(size);
  return EnqueueStatus::kOk;
}

inline usize SpscQueue::available_producer() const {
  return capacity_ - size_producer();
}

void SpscQueue::wait_for_space_producer(usize size) const {
  u32 spin_count = 0;
  while (available_producer() < size) {
    if (spin_count < 64) {
    } else if (spin_count < 1024) {
#if FPAG_BUILD_FLAG(IS_ARCH_X86_FAMILY) && FPAG_BUILD_FLAG(IS_COMPILER_GCC)
      __builtin_ia32_pause();
#else
      std::this_thread::yield();
#endif
    } else if (spin_count < 1024 * 1024) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128));
    } else if (spin_count < 1024 * 1024 * 1024) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128 * 1024));
    } else {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128 * 1024 * 1024));
    }
    ++spin_count;
  }
}

}  // namespace base
