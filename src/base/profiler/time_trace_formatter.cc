// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/profiler/time_trace_formatter.h"

#include <cstdio>
#include <iterator>
#include <span>
#include <string_view>

#include "fmt/base.h"
#include "fmt/compile.h"
#include "fmt/format.h"
#include "fpag/base/numeric.h"
#include "fpag/base/profiler/profile_event.h"

namespace base {

bool TimeTraceFormatter::write_to_file(
    const std::string_view file_path,
    const std::span<const ProfileEvent> events) {
  // Convert string_view to null-terminated string for fopen safely
  fmt::memory_buffer path_buf;
  fmt::format_to(std::back_inserter(path_buf), "{}\0", file_path);

  FILE* fp = std::fopen(path_buf.data(), "w");
  if (!fp) {
    return false;
  }

  // Large buffer to avoid system call overhead during file writes
  fmt::memory_buffer out;
  out.reserve(static_cast<usize>(128 * 1024));  // 128 KiB buffer

  fmt::format_to(std::back_inserter(out), "{}\n", "{\"traceEvents\":[");

  const usize count = events.size();
  for (usize i = 0; i < count; ++i) {
    const auto& e = events[i];
    const f64 start_us = static_cast<f64>(e.start_time_ns) / 1000.0;
    const f64 dur_us = static_cast<f64>(e.duration_ns) / 1000.0;
    const char* comma = (i + 1 < count) ? "," : "";

    fmt::format_to(
        std::back_inserter(out),
        FMT_COMPILE(
            "  {{\"name\":\"{}\",\"cat\":\"{}\",\"ph\":\"X\",\"ts\":{:.3f},"
            "\"dur\":{:.3f},\"pid\":{},\"tid\":{},\"args\":{{\"file\":\"{}\","
            "\"line\":{}}}}}{}\n"),
        e.name ? e.name : "unnamed", e.category ? e.category : "default",
        start_us, dur_us, e.process_id, e.thread_id, e.location.file,
        e.location.line, comma);

    // Flush to disk when buffer reaches 64 KiB
    if (out.size() >= static_cast<usize>(64 * 1024)) {
      std::fwrite(out.data(), 1, out.size(), fp);
      out.clear();
    }
  }

  fmt::format_to(std::back_inserter(out), "{}\n", "]}");
  if (out.size() > 0) {
    std::fwrite(out.data(), 1, out.size(), fp);
  }

  std::fclose(fp);
  return true;
}

}  // namespace base
