// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <iterator>
#include <string_view>
#include <utility>

#include "fmt/base.h"
#include "fmt/compile.h"
#include "fmt/format.h"
#include "fpag/debug/time_util.h"
#include "fpag/logging/format_buffer.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/sink.h"

namespace logging {

template <Sink S, LogLevel kMinLevel>
class SyncLogger {
 public:
  constexpr SyncLogger() = default;
  constexpr ~SyncLogger() { flush(); }

  constexpr SyncLogger(const SyncLogger&) = delete;
  constexpr SyncLogger& operator=(const SyncLogger&) = delete;

  constexpr SyncLogger(SyncLogger&&) noexcept = default;
  constexpr SyncLogger& operator=(SyncLogger&&) noexcept = default;

  constexpr void init(S&& sink) { sink_ = std::move(sink); }

  constexpr void flush() { sink_.flush(); }

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

    format_buffer format_buf;
    if constexpr (fmt::is_compiled_string<Format>::value) {
      fmt::format_to(std::back_inserter(format_buf), Format{},
                     std::forward<Args>(args)...);
    } else if constexpr (std::is_convertible_v<Format, std::string_view>) {
      const std::string_view fmt = static_cast<std::string_view>(format);
      // fmt::format_to(std::back_inserter(format_buf), fmt,
      //                std::forward<Args>(args)...);
      fmt::vformat_to(std::back_inserter(format_buf), fmt,
                      fmt::make_format_args((args)...));
    } else {
      static_assert(!sizeof(Format*), "Unsupported format string type");
    }
    const std::string_view msg{format_buf.data(), format_buf.size()};

    sink_.log(LogEntry{
        .level = level,
        .message = msg,
        .timestamp_ns = debug::current_timestamp_ns(),
    });
  }

  S sink_;
};

}  // namespace logging
