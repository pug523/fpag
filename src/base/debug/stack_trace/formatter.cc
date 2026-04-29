// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/stack_trace/formatter.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "fmt/format.h"
#include "fpag/base/debug/stack_trace/stack_frame.h"
#include "fpag/base/numeric.h"

namespace base {

void append_frame(std::string* out,
                  const StackTraceFrame& frame,
                  const FrameFormatOptions& opts) {
  if (opts.show_index) {
    out->append(fmt::format("#{:3}  ", frame.index));
  }

  if (opts.show_address) {
    out->append(
        fmt::format("{:#018x}  ", reinterpret_cast<uintptr_t>(frame.address)));
  }

  if (opts.show_function) {
    const std::string_view func =
        !frame.function.empty() ? frame.function : "(unknown)";
    if (opts.show_file_line && !frame.file.empty()) {
      out->append(fmt::format("{:<60}", func));
    } else {
      out->append(func);
    }
  }

  if (opts.show_file_line && !frame.file.empty()) {
    // out->append("\n      at ");
    out->append(" at ");
    out->append(frame.file);
    if (frame.line > 0) {
      out->append(fmt::format(":{}", frame.line));
      if (frame.column > 0) {
        out->append(fmt::format(":{}", frame.column));
      }
    }
  }
}

std::string format_frames(const StackTraceFrame* frames,
                          usize count,
                          std::string_view prefix,
                          const FrameFormatOptions& opts) {
  std::string out;
  out.reserve(count * 128);

  if (!prefix.empty()) {
    out = fmt::format("{}\n", prefix);
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
