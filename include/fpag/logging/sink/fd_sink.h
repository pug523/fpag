// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/debug/check.h"
#include "fpag/io/io_util.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/log_level_util.h"
#include "fpag/logging/sink/sink.h"
#include "fpag/term/color_style.h"

namespace logging {

// A sink to one of the process's standard descriptors. The descriptor is
// the sink's type, so the destination is fixed where the logger is built
// and log() stays a direct call on the concrete sink. Two streams, two
// types: program output belongs on stdout and diagnostics on stderr, and
// a logger that has to choose keeps the choice out of the hot path.
template <i32 FD>
class FdSink {
 public:
  explicit FdSink(char* buffer_ptr = nullptr,
                  usize buffer_capacity = 0,
                  term::ColorStyle color_style = term::ColorStyle::Ansi16,
                  bool use_buffer = false)
      : buffer_(buffer_ptr),
        capacity_(buffer_capacity),
        color_style_(color_style),
        use_buffer_(use_buffer) {
    if (use_buffer) {
      FPAG_DCHECK(buffer_);
      FPAG_DCHECK_GT(capacity_, usize{0});
    }
  }

  ~FdSink() = default;

  FdSink(FdSink&&) noexcept = default;
  FdSink& operator=(FdSink&&) noexcept = default;

  void log(const LogEntry& entry) {
    // Prefix is " info: ", "error: ", etc.
    const std::string_view prefix = log_prefix(entry.level, color_style_);

    if (!use_buffer_ && !prefix.empty()) {
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
      io::write(FD, buffer_, offset_);
      offset_ = 0;
    }
  }

 private:
  inline usize available() const { return capacity_ - offset_; }

  inline void directly_write(const std::string_view& prefix,
                             const std::string_view& message) {
    io::write(FD, prefix.data(), prefix.size());
    io::write(FD, message.data(), message.size());
    io::write(FD, "\n", 1);
  }

  char* buffer_;
  usize capacity_;
  usize offset_ = 0;
  term::ColorStyle color_style_;
  bool use_buffer_;
};

using StdoutSink = FdSink<io::STDOUT_FD>;
using StderrSink = FdSink<io::STDERR_FD>;

static_assert(Sink<StdoutSink>);
static_assert(Sink<StderrSink>);

}  // namespace logging
