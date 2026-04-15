// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "logging/async/async_logger.h"

#include "benchmark/benchmark.h"
#include "logging/log_level.h"

namespace logging {

namespace {

// NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)
void async_logger_log_literal_string(benchmark::State& state) {
  const u64 capacity = 1 << 20;
  const u32 num_ops = 100000;
  AsyncLogger logger;
  logger.init(LogLevel::Info, capacity);
  logger.start_backend_worker();
  for (auto _ : state) {
    for (u32 i = 0; i < num_ops; ++i) {
      logger.info("test");
    }
  }
  logger.stop_backend_worker();
}
BENCHMARK(async_logger_log_literal_string);
// NOLINTEND(clang-analyzer-deadcode.DeadStores)

}  // namespace

}  // namespace logging
