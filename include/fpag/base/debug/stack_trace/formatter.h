// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "fpag/base/debug/stack_trace/stack_frame.h"
#include "fpag/base/numeric.h"

namespace base {

// Controls what information is included when formatting a frame.
struct FrameFormatOptions {
  bool show_index : 1 = true;
  bool show_address : 1 = true;
  bool show_function : 1 = true;
  bool show_file_line : 1 = true;
  bool show_module : 1 = false;  // requires address_map lookup; off by default
};

// Appends the formatted representation of `frame` into `out`.
void append_frame(std::string* out,
                  const StackTraceFrame& frame,
                  const FrameFormatOptions& opts = {});

// Formats `count` frames from `frames` into a multi-line string, one frame per
// line, optionally preceded by `prefix` on the first line.
std::string format_frames(const StackTraceFrame* frames,
                          usize count,
                          std::string_view prefix = "",
                          const FrameFormatOptions& opts = {});

// // Formats a single frame into a human-readable string.
// // Example output (default options):
// //   #  3  0x00007f3a1b2c3d4e  base::StackTrace::collect_trace()
// //           /home/user/src/base/debug/stack_trace/formatter.cc:21
// std::string format_frame(const StackTraceFrame& frame,
//                          const FrameFormatOptions& opts = {});

}  // namespace base
