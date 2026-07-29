// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/stack_trace/formatter.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "fmt/format.h"
#include "fpag/base/numeric.h"
#include "fpag/debug/stack_trace/stack_frame.h"

namespace debug {

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

  const std::string_view file = frame.location.file_name();
  std::string_view func = frame.location.function_name();

  if (opts.show_function) {
    if (func.empty()) {
      func = "(unknown)";
    }
    if (opts.show_file_line && !file.empty()) {
      out->append(fmt::format("{:<60}", func));
    } else {
      out->append(func);
    }
  }

  if (opts.show_file_line && !file.empty()) {
    // if (file.empty()) {
    //   file = "(unknown)";
    // }
    // out->append("\n      at ");
    out->append(" at ");
    out->append(file);
    if (frame.location.valid_line()) {
      out->append(fmt::format(":{}", frame.location.line));
      if (frame.location.valid_column()) {
        out->append(fmt::format(":{}", frame.location.column));
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

}  // namespace debug
