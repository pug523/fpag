// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <utility>

#include "fpag/base/numeric.h"
#include "fpag/container/spsc_queue.h"
#include "fpag/logging/async/backend_worker.h"
#include "fpag/logging/async/serializer.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/sink.h"
#include "fpag/logging/wait_strategy.h"
#include "fpag/str/string_interner.h"

namespace logging {

template <Sink S,
          LogLevel kMinLevel,
          bool kUseInterner = true,
          WaitStrategy W = BusySpin>
class AsyncLogger {
 public:
  AsyncLogger() : interner_(4096) {}
  ~AsyncLogger() { reset(); }

  AsyncLogger(const AsyncLogger&) = delete;
  AsyncLogger& operator=(const AsyncLogger&) = delete;

  AsyncLogger(AsyncLogger&&) noexcept = default;
  AsyncLogger& operator=(AsyncLogger&&) noexcept = default;

  constexpr void init(
      S&& sink,
      usize interner_map_capacity = static_cast<usize>(16 * 1024),
      usize queue_capacity = container::SpscQueue::default_capacity(),
      container::SpscQueue::Mode mode = container::SpscQueue::Mode::Default) {
    interner_.init(interner_map_capacity);
    worker_.init(std::move(sink), &interner_, queue_capacity, mode);
  }
  constexpr void start_backend_worker() { worker_.start(); }
  constexpr void stop_backend_worker() { worker_.stop(); }
  constexpr void force_stop_backend_worker() { worker_.force_stop(); }
  constexpr void flush() { worker_.flush(); }
  constexpr void reset() { worker_.reset(); }

  constexpr usize dropped_count() const {
    return worker_.queue()->dropped_count();
  }
  constexpr usize blocked_count() const {
    return worker_.queue()->blocked_count();
  }

  template <typename Format, typename... Args>
  void trace(Format fmt, Args&&... args) {
    log<LogLevel::Trace>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  void debug(Format fmt, Args&&... args) {
    log<LogLevel::Debug>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  void info(Format fmt, Args&&... args) {
    log<LogLevel::Info>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  void warn(Format fmt, Args&&... args) {
    log<LogLevel::Warn>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  void error(Format fmt, Args&&... args) {
    log<LogLevel::Error>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  void fatal(Format fmt, Args&&... args) {
    log<LogLevel::Fatal>(fmt, std::forward<Args>(args)...);
  }

  template <typename Format, typename... Args>
  void wo_prefix(Format fmt, Args&&... args) {
    log<LogLevel::WithoutPrefix>(fmt, std::forward<Args>(args)...);
  }

 private:
  static consteval bool should_log(LogLevel level) {
    return level >= kMinLevel;
  }

  template <LogLevel level, typename Format, typename... Args>
  void log(Format format, Args&&... args) {
    if constexpr (!should_log(level)) {
      return;
    }
    Serializer<Format, kUseInterner, Args&&...>::serialize_to(
        level, &interner_, worker_.queue(), format,
        std::forward<Args>(args)...);
  }

  BackendWorker<S, W> worker_;
  str::StringInterner interner_;
};

}  // namespace logging
