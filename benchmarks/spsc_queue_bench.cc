// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "benchmark/benchmark.h"
#include "fpag/base/debug/dlog.h"
#include "fpag/base/debug/logger.h"
#include "fpag/base/spsc_queue.h"

namespace base {

namespace {

// NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)
void spsc_queue_simple_enqueue_single_thread(benchmark::State& state) {
  SpscQueue queue;
  constexpr usize kCap = 1 << 30;
  queue.init(kCap, SpscQueue::Mode::kDrop);

  u64 sample_data = 168;
  for (auto _ : state) {
    queue.enqueue(&sample_data, sizeof(sample_data));
  }

  if (queue.dropped_count() > 0) {
    base::debug_logger().warn("spsc queue dropped {} entries",
                              queue.dropped_count());
  }
}
BENCHMARK(spsc_queue_simple_enqueue_single_thread);
// NOLINTEND(clang-analyzer-deadcode.DeadStores)

}  // namespace

}  // namespace base

