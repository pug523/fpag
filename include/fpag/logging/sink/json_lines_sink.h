// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fmt/base.h"
#include "fmt/compile.h"
#include "fpag/base/memory_mapped_stream_writer.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/log_level_util.h"
#include "fpag/logging/sink/sink.h"

namespace logging {

class JsonLinesSink {
 public:
  JsonLinesSink() = default;

  explicit JsonLinesSink(std::string_view file_path,
                         usize initial_capacity = static_cast<usize>(1024 *
                                                                     1024)) {
    writer_.open(file_path, initial_capacity);
  }

  ~JsonLinesSink() { flush(); }

  JsonLinesSink(const JsonLinesSink&) = delete;
  JsonLinesSink& operator=(const JsonLinesSink&) = delete;

  JsonLinesSink(JsonLinesSink&&) noexcept = default;
  JsonLinesSink& operator=(JsonLinesSink&&) noexcept = default;

  bool open(std::string_view file_path,
            usize initial_capacity = static_cast<usize>(1024 * 1024)) {
    return writer_.open(file_path, initial_capacity);
  }

  void log(const LogEntry& entry) {
    constexpr usize kMaxLogJsonSize = 4096;
    u8* dest = writer_.prepare_write_buffer(kMaxLogJsonSize);
    if (!dest) [[unlikely]] {
      return;
    }

    const std::string_view level_str = log_level_to_string(entry.level);

    // Format directly into mapped region (Zero-allocation)
    // TODO(logging): Include SourceLocation once added to LogEntry:
    // ,\"location\":\"{}:{}\"", entry.location.file, entry.location.line
    const auto result = fmt::format_to_n(
        reinterpret_cast<char*>(dest), kMaxLogJsonSize,
        FMT_COMPILE("{{\"ts\":{},\"level\":\"{}\",\"msg\":\"{}\"}}\n"),
        entry.timestamp_ns, level_str, entry.message);

    writer_.commit_write(result.size);
  }

  void flush() { writer_.finish(); }

 private:
  base::MemoryMappedStreamWriter writer_;
};

static_assert(Sink<JsonLinesSink>);

}  // namespace logging
