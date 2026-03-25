// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstring>
#include <format>
#include <string_view>
#include <utility>

#include "base/debug/check.h"
#include "base/logging/log_level.h"
#include "base/numeric.h"

namespace base {

// Thread-safe simple logger that writes to a shared buffer.
class SyncLogger {
 public:
  explicit SyncLogger(char* buffer_ptr,
                      usize size = 4096,
                      LogLevel min_level = LogLevel::Info,
                      bool use_ansi_style = true)
      : buffer_(buffer_ptr),
        capacity_(size),
        min_level_(min_level),
        use_ansi_style_(use_ansi_style) {
    dcheck(buffer_);
    dcheck_gt(capacity_, usize(0));
    lock_.clear();
  }
  ~SyncLogger() { flush(); }

  SyncLogger(const SyncLogger&) = delete;
  SyncLogger& operator=(const SyncLogger&) = delete;

  SyncLogger(SyncLogger&& other) noexcept;
  SyncLogger& operator=(SyncLogger&& other) noexcept;

  constexpr static usize kLocalStackBufSize = 512;
  template <typename... Args>
  inline void log(LogLevel level,
                  std::format_string<Args&&...> fmt,
                  Args&&... args) {
    if (!should_log(level)) {
      return;
    }

    char stack_buf[kLocalStackBufSize];

    const std::string_view prefix = log_prefix(level, use_ansi_style_);
    dcheck_lt(prefix.size(), sizeof(stack_buf));
    std::memcpy(stack_buf, prefix.data(), prefix.size());

    const std::format_to_n_result result =
        std::format_to_n(stack_buf + prefix.size(),
                         static_cast<i32>(sizeof(stack_buf) - prefix.size()),
                         fmt, std::forward<Args>(args)...);
    const usize len = static_cast<usize>(result.out - stack_buf);

    write_to_shared_buffer(stack_buf, len);
  }

  template <typename... Args>
  inline void trace(std::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Trace, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void debug(std::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void info(std::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void warn(std::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void error(std::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Error, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void fatal(std::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Fatal, fmt, std::forward<Args>(args)...);
  }

  void flush();

 private:
  void write_to_shared_buffer(const char* data, usize len);
  void spin_lock();
  inline void spin_unlock() { lock_.clear(std::memory_order_release); }

  inline constexpr bool should_log(LogLevel level) {
    return level >= min_level_;
  }

  char* const buffer_;
  const usize capacity_;
  usize offset_ = 0;
  LogLevel min_level_;
  bool use_ansi_style_;
  std::atomic_flag lock_;
};

SyncLogger& global_logger();

}  // namespace base
