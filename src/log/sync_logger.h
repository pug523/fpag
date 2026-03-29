// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include "base/debug/check.h"
#include "base/numeric.h"
#include "log/log_level.h"
#include "str/format_util.h"

namespace base {

// Thread-safe simple logger for writing log messages with newline to stdout.
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

  template <typename... Args>
  inline void trace(str::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Trace, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void debug(str::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void info(str::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void warn(str::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void error(str::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Error, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void fatal(str::format_string<Args&&...> fmt, Args&&... args) {
    log(LogLevel::Fatal, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void raw(str::format_string<Args&&...> fmt, Args&&... args) {
    log_raw(fmt, std::forward<Args>(args)...);
  }

  inline void raw_str(std::string_view str) { log_raw_str(str); }

  void flush();

 private:
  void write_to_shared_buffer(const char* data, usize len);
  void spin_lock();
  inline void spin_unlock() { lock_.clear(std::memory_order_release); }

  inline constexpr bool should_log(LogLevel level) {
    return level >= min_level_;
  }

  // Local stack buffer size used for formatting log messages.
  // This affects the maximum log message size.
  constexpr static usize kLocalStackBufSize = 4096;

  template <typename... Args>
  inline void log(LogLevel level,
                  str::format_string<Args&&...> fmt,
                  Args&&... args) {
    if (!should_log(level)) {
      return;
    }

    char stack_buf[kLocalStackBufSize];

    // Prefix is " info: ", "error: ", etc.
    const std::string_view prefix = log_prefix(level, use_ansi_style_);
    dcheck_lt(prefix.size(), sizeof(stack_buf));
    std::memcpy(stack_buf, prefix.data(), prefix.size());

    // -1 for '\n'
    const usize max_len = sizeof(stack_buf) - prefix.size() - 1;
    const str::format_to_n_result result = str::format_to_n(
        stack_buf + prefix.size(), max_len, fmt, std::forward<Args>(args)...);

    const usize result_size = static_cast<usize>(result.size);
    const usize payload_size = result_size > max_len ? max_len : result_size;
    // const bool truncated = result_size > max_len;
    const usize len = prefix.size() + payload_size + 1;

    stack_buf[prefix.size() + payload_size] = '\n';
    write_to_shared_buffer(stack_buf, len);
  }

  template <typename... Args>
  inline void log_raw(str::format_string<Args&&...> fmt, Args&&... args) {
    char stack_buf[kLocalStackBufSize];
    const str::format_to_n_result result = str::format_to_n(
        stack_buf, sizeof(stack_buf) - 1, fmt, std::forward<Args>(args)...);
    const usize len = static_cast<usize>(result.size) + 1;
    stack_buf[static_cast<usize>(result.size)] = '\n';
    write_to_shared_buffer(stack_buf, len);
  }

  inline void log_raw_str(std::string_view str) {
    char stack_buf[kLocalStackBufSize];
    const usize len =
        str.size() > sizeof(stack_buf) - 1 ? sizeof(stack_buf) - 1 : str.size();
    std::memcpy(stack_buf, str.data(), len);
    stack_buf[len] = '\n';
    write_to_shared_buffer(stack_buf, len + 1);
  }

  char* buffer_;
  usize capacity_;
  usize offset_ = 0;
  LogLevel min_level_;
  bool use_ansi_style_;
  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};

SyncLogger& global_logger();

}  // namespace base
