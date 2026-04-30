// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <utility>

#include "fpag/base/numeric.h"
#include "fpag/base/spsc_queue.h"
#include "fpag/logging/async/backend_worker.h"
#include "fpag/logging/async/serializer.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/sink.h"
#include "fpag/str/string_interner.h"

namespace logging {

template <IsSink S, LogLevel min_level>
class AsyncLogger {
 public:
  AsyncLogger() : interner_(4096) {}
  ~AsyncLogger() { reset(); }

  AsyncLogger(const AsyncLogger&) = delete;
  AsyncLogger& operator=(const AsyncLogger&) = delete;

  AsyncLogger(AsyncLogger&&) noexcept = default;
  AsyncLogger& operator=(AsyncLogger&&) noexcept = default;

  inline void init(
      S&& sink,
      usize interner_map_capacity = 16 * 1024,
      usize queue_capacity = base::SpscQueue::kDefaultCapacity,
      base::SpscQueue::Mode mode = base::SpscQueue::Mode::kDefault) {
    interner_.init(interner_map_capacity);
    worker_.init(std::move(sink), &interner_, queue_capacity, mode);
  }
  inline void start_backend_worker() { worker_.start(); }
  inline void stop_backend_worker() { worker_.stop(); }
  inline void force_stop_backend_worker() { worker_.force_stop(); }
  inline void flush() { worker_.flush(); }
  inline void reset() { worker_.reset(); }

  template <typename Format, typename... Args>
  inline void trace(Format fmt, Args&&... args) {
    log<LogLevel::Trace>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  inline void debug(Format fmt, Args&&... args) {
    log<LogLevel::Debug>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  inline void info(Format fmt, Args&&... args) {
    log<LogLevel::Info>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  inline void warn(Format fmt, Args&&... args) {
    log<LogLevel::Warn>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  inline void error(Format fmt, Args&&... args) {
    log<LogLevel::Error>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  inline void fatal(Format fmt, Args&&... args) {
    log<LogLevel::Fatal>(fmt, std::forward<Args>(args)...);
  }

 private:
  inline static consteval bool should_log(LogLevel level) {
    return level >= min_level;
  }

  template <LogLevel level, typename Format, typename... Args>
  inline void log(Format format, Args&&... args) {
    if constexpr (!should_log(level)) {
      return;
    }
    Serializer<Format, Args&&...>::serialize_to(level, &interner_,
                                                worker_.queue(), format,
                                                std::forward<Args>(args)...);
  }

  BackendWorker<S> worker_;
  str::StringInterner interner_;
};

}  // namespace logging
