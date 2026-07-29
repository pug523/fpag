// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/container/spsc_queue.h"

namespace logging {

struct BusySpin {
  u32 spin_count = 0;

  constexpr void notify() noexcept {}

  void wait() noexcept {
    if (spin_count < 64) {
    } else if (spin_count < 1024) {
#if FPAG_BUILD_FLAG(IS_ARCH_X86_FAMILY) && FPAG_BUILD_FLAG(IS_COMPILER_GCC)
      __builtin_ia32_pause();
#else
      std::this_thread::yield();
#endif
    } else {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128));
    }
    ++spin_count;
  }

  void wait_for_next(const container::SpscQueue& /* queue */,
                     const std::atomic<bool>& /* stopping */) noexcept {
    wait();
  }

  void wait_for_flush(const container::SpscQueue& /* queue */,
                      const std::atomic<bool>& /* stopping */) noexcept {
    wait();
  }

  constexpr void reset() noexcept { spin_count = 0; }
};

struct Blocking {
  std::mutex mutex;
  std::condition_variable cv;

  void notify() noexcept { cv.notify_one(); }

  void wait_for_next(const container::SpscQueue& queue,
                     const std::atomic<bool>& stopping) noexcept {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(10), [&] {
      return !queue.empty() || stopping.load(std::memory_order_relaxed);
    });
  }

  void wait_for_flush(const container::SpscQueue& queue,
                      const std::atomic<bool>& /* stopping */) noexcept {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(10),
                [&] { return queue.empty(); });
  }

  constexpr void reset() noexcept {}
};

template <typename T>
concept WaitStrategy = requires(T& waiter,
                                const container::SpscQueue& queue,
                                const std::atomic<bool>& stopping) {
  waiter.notify();
  waiter.wait_for_next(queue, stopping);
  waiter.wait_for_flush(queue, stopping);
  waiter.reset();
};

}  // namespace logging
