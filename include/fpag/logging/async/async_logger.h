// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <memory>
#include <utility>

#include "fpag/base/numeric.h"
#include "fpag/base/spsc_queue.h"
#include "fpag/logging/async/backend_worker.h"
#include "fpag/logging/async/serializer.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/sink.h"
#include "fpag/str/format_util.h"

namespace logging {

class AsyncLogger {
 public:
  AsyncLogger() = default;
  ~AsyncLogger() { reset(); }

  AsyncLogger(const AsyncLogger&) = delete;
  AsyncLogger& operator=(const AsyncLogger&) = delete;

  AsyncLogger(AsyncLogger&& other) noexcept = default;
  AsyncLogger& operator=(AsyncLogger&& other) noexcept = default;

  void init(LogLevel min_level = kDefaultLogLevel,
            usize queue_capacity = base::SpscQueue::kDefaultCapacity,
            base::SpscQueue::Mode mode = base::SpscQueue::Mode::kDefault);
  void register_sink(std::unique_ptr<Sink> sink);
  void start_backend_worker();
  void stop_backend_worker();
  void force_stop_backend_worker();
  void flush();
  void reset();

  template <typename... Args>
  inline constexpr void trace(str::format_string<Args&&...> fmt,
                              Args&&... args) {
    log(LogLevel::Trace, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline constexpr void debug(str::format_string<Args&&...> fmt,
                              Args&&... args) {
    log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline constexpr void info(str::format_string<Args&&...> fmt,
                             Args&&... args) {
    log(LogLevel::Info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline constexpr void warn(str::format_string<Args&&...> fmt,
                             Args&&... args) {
    log(LogLevel::Warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline constexpr void error(str::format_string<Args&&...> fmt,
                              Args&&... args) {
    log(LogLevel::Error, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline constexpr void fatal(str::format_string<Args&&...> fmt,
                              Args&&... args) {
    log(LogLevel::Fatal, fmt, std::forward<Args>(args)...);
  }

 private:
  inline constexpr bool should_log(LogLevel level) {
    return level >= min_level_;
  }

  // Local stack buffer size used for formatting log messages.
  // This affects the maximum log message size.
  constexpr static usize kLocalStackBufSize = 4096;

  template <typename... Args>
  inline constexpr void log(LogLevel level,
                            str::format_string<Args&&...> fmt,
                            Args&&... args) {
    if (!should_log(level)) {
      return;
    }
    Serializer<Args&&...>::serialize_to(level, worker_.queue(), fmt,
                                        std::forward<Args>(args)...);
  }

  BackendWorker worker_;
  LogLevel min_level_;
};

AsyncLogger& global_async_logger();

}  // namespace logging
