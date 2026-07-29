// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/profiler/time_trace_formatter.h"

#include <span>
#include <string_view>

#include "fmt/base.h"
#include "fmt/compile.h"
#include "fpag/base/numeric.h"
#include "fpag/debug/profiler/profile_event.h"
#include "fpag/io/memory_mapped_stream_writer.h"

namespace base {

// static
bool TimeTraceFormatter::write_to_file(
    const std::string_view file_path,
    const std::span<const ProfileEvent> events) {
  MemoryMappedStreamWriter writer;

  // Initial allocation hint: ~200 bytes per event + JSON header/footer wrapper.
  const usize estimated_size = events.size() * 200 + 512;
  if (!writer.open(file_path, estimated_size)) {
    return false;
  }

  constexpr std::string_view kHeader = "{\"traceEvents\":[\n";
  if (!writer.write(kHeader.data(), kHeader.size())) {
    return false;
  }

  constexpr usize kMaxSingleEventWriteSize = 1024;
  const usize count = events.size();

  for (usize i = 0; i < count; ++i) {
    const auto& e = events[i];
    const f64 start_us = static_cast<f64>(e.start_time_ns) / 1000.0;
    const f64 dur_us = static_cast<f64>(e.duration_ns) / 1000.0;
    const char* comma = (i + 1 < count) ? "," : "";

    // Obtain direct pointer to memory-mapped region.
    u8* dest_ptr = writer.prepare_write_buffer(kMaxSingleEventWriteSize);
    if (!dest_ptr) {
      return false;
    }

    // Format directly into mapped file buffer (zero-copy / zero-allocation).
    auto result = fmt::format_to_n(
        reinterpret_cast<char*>(dest_ptr), kMaxSingleEventWriteSize,
        FMT_COMPILE(
            "  {{\"name\":\"{}\",\"cat\":\"{}\",\"ph\":\"X\",\"ts\":{:.3f},"
            "\"dur\":{:.3f},\"pid\":{},\"tid\":{},\"args\":{{\"file\":\"{}\","
            "\"line\":{}}}}}{}\n"),
        e.name ? e.name : "unnamed", e.category ? e.category : "default",
        start_us, dur_us, e.process_id, e.thread_id, e.location.file,
        e.location.line, comma);

    writer.commit_write(result.size);
  }

  constexpr std::string_view kFooter = "]}\n";
  if (!writer.write(kFooter.data(), kFooter.size())) {
    return false;
  }

  // Truncate to exact size and flush pages asynchronously.
  writer.finish();
  return true;
}

}  // namespace base
