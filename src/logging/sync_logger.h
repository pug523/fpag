// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/numeric.h"
#include "base/time_util.h"
#include "logging/log_entry.h"
#include "logging/log_level.h"
#include "logging/sink/sink.h"
#include "str/format_util.h"

namespace logging {

class SyncLogger {
 public:
  SyncLogger() = default;
  ~SyncLogger() { reset(); }

  SyncLogger(const SyncLogger&) = delete;
  SyncLogger& operator=(const SyncLogger&) = delete;

  SyncLogger(SyncLogger&& other) noexcept = default;
  SyncLogger& operator=(SyncLogger&& other) noexcept = default;

  void init(LogLevel min_level = LogLevel::Info);
  void register_sink(std::unique_ptr<Sink> sink);

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

    // TODO: Add memory buffer as member field to enable log buffering.
    char stack_buf[kLocalStackBufSize];
    const str::format_to_n_result result = str::format_to_n(
        stack_buf, kLocalStackBufSize, fmt, std::forward<Args>(args)...);
    const usize size = static_cast<usize>(result.size);
    const std::string_view msg{stack_buf, size};
    const LogEntry entry{
        .level = level,
        .message = msg,
        .timestamp_ns = base::current_timestamp_ns(),
    };

    for (const std::unique_ptr<Sink>& sink : sinks_) {
      sink->log(entry);
    }
  }

  std::vector<std::unique_ptr<Sink>> sinks_;
  LogLevel min_level_;
};

SyncLogger& global_sync_logger();

}  // namespace logging
