// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "benchmark/benchmark.h"
#include "fmt/compile.h"
#include "fpag/base/debug/logger.h"
#include "fpag/base/numeric.h"
#include "fpag/base/spsc_queue.h"
#include "fpag/logging/async/async_logger.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/logger.h"
#include "fpag/logging/sink/null_sink.h"

namespace logging {

namespace {

template <Logger L>
void setup(L* logger,
           usize map_capacity,
           usize queue_capacity,
           base::SpscQueue::Mode queue_mode) {
  logger->init({}, map_capacity, queue_capacity, queue_mode);
  logger->start_backend_worker();
}

template <Logger L>
void cleanup(L* logger, usize dropped_count) {
  logger->stop_backend_worker();
  if (dropped_count > 0) {
    base::debug_logger().warn("async logger dropped {} logs", dropped_count);
    base::debug_logger().flush();
  }
}

constexpr usize kMapCap = 1 << 16;
constexpr usize kQueueCap = 1 << 30;
constexpr base::SpscQueue::Mode kMode = base::SpscQueue::Mode::kDrop;

// NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)
void async_logger_log_literal_compiled_string_w_interner(
    benchmark::State& state) {
  AsyncLogger<NullSink, LogLevel::Info, true> logger;
  setup(&logger, kMapCap, kQueueCap, kMode);
  for (auto _ : state) {
    logger.info(FMT_COMPILE("test"));
  }
  cleanup(&logger, logger.dropped_count());
}
BENCHMARK(async_logger_log_literal_compiled_string_w_interner);

void async_logger_log_literal_compiled_string_wo_interner(
    benchmark::State& state) {
  AsyncLogger<NullSink, LogLevel::Info, false> logger;
  setup(&logger, kMapCap, kQueueCap, kMode);
  for (auto _ : state) {
    logger.info(FMT_COMPILE("test"));
  }
  cleanup(&logger, logger.dropped_count());
}
BENCHMARK(async_logger_log_literal_compiled_string_wo_interner);

void async_logger_log_literal_string_w_interner(benchmark::State& state) {
  AsyncLogger<NullSink, LogLevel::Info, true> logger;
  setup(&logger, kMapCap, kQueueCap, kMode);
  for (auto _ : state) {
    logger.info("test");
  }
  cleanup(&logger, logger.dropped_count());
}
BENCHMARK(async_logger_log_literal_string_w_interner);

void async_logger_log_literal_string_wo_interner(benchmark::State& state) {
  AsyncLogger<NullSink, LogLevel::Info, false> logger;
  setup(&logger, kMapCap, kQueueCap, kMode);
  for (auto _ : state) {
    logger.info("test");
  }
  cleanup(&logger, logger.dropped_count());
}
BENCHMARK(async_logger_log_literal_string_wo_interner);
// NOLINTEND(clang-analyzer-deadcode.DeadStores)

}  // namespace

}  // namespace logging
