// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/logging/async/async_logger.h"

#include <memory>
#include <utility>

#include "fpag/base/console.h"
#include "fpag/base/numeric.h"
#include "fpag/base/spsc_queue.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/sink.h"
#include "fpag/logging/sink/stdout_sink.h"
#include "fpag/mem/page_allocator.h"

namespace logging {

void AsyncLogger::init(LogLevel min_level,
                       usize queue_capacity,
                       base::SpscQueue::Mode mode) {
  min_level_ = min_level;
  worker_.init(queue_capacity, mode);
}

void AsyncLogger::register_sink(std::unique_ptr<Sink> sink) {
  worker_.register_sink(std::move(sink));
}

void AsyncLogger::start_backend_worker() {
  if (!worker_.running()) [[likely]] {
    worker_.start();
  }
}

void AsyncLogger::stop_backend_worker() {
  if (worker_.running()) [[likely]] {
    worker_.stop();
  }
}

void AsyncLogger::force_stop_backend_worker() {
  if (worker_.running()) [[likely]] {
    worker_.force_stop();
  }
}

void AsyncLogger::flush() {
  if (worker_.running()) [[likely]] {
    worker_.flush();
  }
}

void AsyncLogger::reset() {
  flush();
  stop_backend_worker();
  worker_.reset();
}

AsyncLogger& global_async_logger() {
  static AsyncLogger logger = [] {
    AsyncLogger l;
    l.init(kDefaultLogLevel);
    l.register_sink(std::make_unique<StdoutSink>(
        static_cast<char*>(mem::allocate_pages(mem::kPageSize)), mem::kPageSize,
        is_ansi_escape_sequence_available(base::Stream::Stdout), true));
    l.start_backend_worker();
    return l;
  }();
  return logger;
}

}  // namespace logging
