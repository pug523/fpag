// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "benchmark/benchmark.h"
#include "fmt/compile.h"
#include "fpag/base/numeric.h"
#include "fpag/base/spsc_queue.h"
#include "fpag/logging/async/async_logger.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/null_sink.h"

namespace logging {

namespace {

// NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)
void async_logger_log_literal_string(benchmark::State& state) {
  const usize map_capacity = 1 << 16;
  const usize queue_capacity = 1 << 20;
  AsyncLogger<NullSink, LogLevel::Info> logger;
  logger.init({}, map_capacity, queue_capacity, base::SpscQueue::Mode::kDrop);
  logger.start_backend_worker();
  // usize i = 0;
  for (auto _ : state) {
    logger.info(FMT_COMPILE("test"));
    // }
  }
  logger.stop_backend_worker();
}
BENCHMARK(async_logger_log_literal_string);
// NOLINTEND(clang-analyzer-deadcode.DeadStores)

}  // namespace

}  // namespace logging
