// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <string_view>

#include "fpag/base/color_mode.h"
#include "fpag/base/debug/check.h"
#include "fpag/base/io_util.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/sink.h"

namespace logging {

class StdoutSink final : public Sink<StdoutSink> {
 public:
  explicit StdoutSink(char* buffer_ptr = nullptr,
                      usize buffer_capacity = 0,
                      base::ColorMode color_mode = base::ColorMode::Ansi16,
                      bool use_buffer = false)
      : buffer_(buffer_ptr),
        capacity_(buffer_capacity),
        color_mode_(color_mode),
        use_buffer_(use_buffer) {
    if (use_buffer) {
      FPAG_DCHECK(buffer_);
      FPAG_DCHECK_GT(capacity_, usize{0});
    }
  }

  ~StdoutSink() = default;

  StdoutSink(StdoutSink&&) noexcept = default;
  StdoutSink& operator=(StdoutSink&&) noexcept = default;

  void log(const LogEntry& entry) {
    // Prefix is " info: ", "error: ", etc.
    const std::string_view prefix = log_prefix(entry.level, color_mode_);

    if (!use_buffer_) {
      directly_write(prefix, entry.message);
      return;
    }

    // +1 for '\n'
    const usize total_len = prefix.size() + entry.message.size() + 1;

    if (total_len <= available()) {
      std::memcpy(buffer_ + offset_, prefix.data(), prefix.size());
      std::memcpy(buffer_ + offset_ + prefix.size(), entry.message.data(),
                  entry.message.size());
      buffer_[offset_ + total_len - 1] = '\n';
      offset_ += total_len;
    } else {
      flush();
      if (total_len <= capacity_) {
      } else {
        directly_write(prefix, entry.message);
      }
    }
  }

  void flush() {
    if (offset_ > 0 && use_buffer_) [[likely]] {
      base::write(base::kStdoutFd, buffer_, offset_);
      offset_ = 0;
    }
  }

 private:
  inline usize available() const { return capacity_ - offset_; }

  inline void directly_write(const std::string_view& prefix,
                             const std::string_view& message) {
    base::write(base::kStdoutFd, prefix.data(), prefix.size());
    base::write(base::kStdoutFd, message.data(), message.size());
    base::write(base::kStdoutFd, "\n", 1);
  }

  char* buffer_;
  usize capacity_;
  usize offset_ = 0;
  base::ColorMode color_mode_;
  bool use_buffer_;
};

}  // namespace logging
