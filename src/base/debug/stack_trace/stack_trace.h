// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "base/debug/stack_trace/stack_frame.h"
#include "base/numeric.h"
#include "build/build_config.h"

namespace base {

enum class StackTraceStatus : u8 {
  Uninitialized,
  Initialized,
  Collected,
  Failed,
};

class StackTrace {
 public:
  StackTrace() = default;
  ~StackTrace() = default;

  StackTrace(const StackTrace&) = delete;
  StackTrace& operator=(const StackTrace&) = delete;

#if FPAG_BUILD_FLAG(IS_OS_ANDROID)
  // Android has a issue with deep stack traces, so we limit it to 62.
  static constexpr usize kMaxTraceDepth = 62;
#else
  // Seems reasonable for most cases without being huge.
  static constexpr usize kMaxTraceDepth = 256;
#endif

  // Initializes the stack trace with a buffer of `frames_buf` which has `depth`
  // frames. `depth` must be less than or equal to kMaxTraceDepth.
  void init(StackTraceFrame* frames_buf,
            usize depth = kMaxTraceDepth,
            usize skip = 2);

  // Collects the current stack trace up to the `depth_`.
  // Must be called before `print_trace()` or `to_string()`.
  void collect_trace();

  // Prints the stack trace to the console with a prefix.
  void print_trace(std::string_view prefix = "") const;

  // Returns a string representation of the stack trace.
  std::string to_string() const;

  // Returns the number of frames actually captured (0 before collect_trace).
  usize frame_count() const { return count_; }

  // Returns the captured frames (valid after collect_trace).
  const StackTraceFrame* frames() const { return frames_; }

 private:
  // Copies `str` into string_buffer_ and returns a string_view into it.
  // The view remains valid as long as string_buffer_ is not modified.
  std::string_view intern_string(std::string_view str);

  StackTraceFrame* frames_ = nullptr;
  // Maximum number of frames to capture (set by init()).
  usize depth_ = kMaxTraceDepth;

  // Number of frames to skip before capturing (set by init()).
  usize skip_ = 0;

  // Number of frames actually captured by collect_trace().
  usize count_ = 0;

  // Stable storage for symbolicated strings (function names, file paths).
  // string_view members inside StackTraceFrame point into this buffer.
  std::vector<char> string_buffer_;
  StackTraceStatus status_ = StackTraceStatus::Uninitialized;
};

// Simple helper that prints the current stack trace to the console.
void print_stack_trace_from_here();

}  // namespace base
