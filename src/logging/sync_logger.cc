// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/logging/sync_logger.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>

#include "fpag/base/console.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/sink.h"
#include "fpag/logging/sink/stdout_sink.h"
#include "fpag/mem/page_allocator.h"

namespace logging {

void SyncLogger::init(LogLevel min_level) {
  min_level_ = min_level;
}

void SyncLogger::register_sink(std::unique_ptr<Sink> sink) {
  sinks_.emplace_back(std::move(sink));
}

void SyncLogger::flush() {
  for (const std::unique_ptr<Sink>& sink : sinks_) {
    sink->flush();
  }
}

void SyncLogger::reset() {
  flush();
  sinks_.clear();
  min_level_ = kDefaultLogLevel;
}

SyncLogger& global_sync_logger() {
  static SyncLogger logger = [] {
    SyncLogger l;
    l.init(kDefaultLogLevel);
    l.register_sink(std::make_unique<StdoutSink>(
        static_cast<char*>(mem::allocate_pages(mem::kPageSize)), mem::kPageSize,
        is_ansi_escape_sequence_available(base::Stream::Stdout), true));
    return l;
  }();
  return logger;
}

}  // namespace logging
