// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/stack_trace/formatter.h"

#include <cstdint>
#include <format>
#include <string>

#include "base/debug/stack_trace/stack_frame.h"
#include "base/numeric.h"

namespace base {

void append_frame(std::string* out,
                  const StackTraceFrame& frame,
                  const FrameFormatOptions& opts) {
  if (opts.show_index) {
    out->append(std::format("#{:3}  ", frame.index));
  }

  if (opts.show_address) {
    out->append(
        std::format("{:#018x}  ", reinterpret_cast<uintptr_t>(frame.address)));
  }

  if (opts.show_function) {
    out->append(!frame.function.empty() ? frame.function : "(unknown)");
  }

  if (opts.show_file_line && !frame.file.empty()) {
    out->append("\n      at ");
    out->append(frame.file);
    if (frame.line > 0) {
      out->append(std::format(":{}", frame.line));
      if (frame.column > 0) {
        out->append(std::format(":{}", frame.column));
      }
    }
  }
}

std::string format_frames(const StackTraceFrame* frames,
                          usize count,
                          std::string_view prefix,
                          const FrameFormatOptions& opts) {
  std::string out;
  if (!prefix.empty()) {
    out = std::format("{}\n", prefix);
  }

  for (usize i = 0; i < count; ++i) {
    append_frame(&out, frames[i], opts);
    out.push_back('\n');
  }
  return out;
}

// std::string format_frame(const StackTraceFrame& frame,
//                          const FrameFormatOptions& opts) {
//   std::string out;
//   out.reserve(128);
//   append_frame(&out, frame, opts);
//   return out;
// }

}  // namespace base
