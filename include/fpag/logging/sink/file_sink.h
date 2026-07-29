// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <string_view>

#include "fmt/base.h"
#include "fmt/compile.h"
#include "fpag/base/numeric.h"
#include "fpag/io/memory_mapped_stream_writer.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/log_level_util.h"
#include "fpag/logging/sink/sink.h"

namespace logging {

class FileSink {
 public:
  FileSink() = default;

  explicit FileSink(std::string_view file_path,
                    usize initial_capacity = static_cast<usize>(1024 * 1024)) {
    writer_.open(file_path, initial_capacity);
  }

  ~FileSink() { flush(); }

  FileSink(const FileSink&) = delete;
  FileSink& operator=(const FileSink&) = delete;

  FileSink(FileSink&&) noexcept = default;
  FileSink& operator=(FileSink&&) noexcept = default;

  void log(const LogEntry& entry) {
    constexpr usize kMaxLogLineSize = 2048;
    u8* dest = writer_.prepare_write_buffer(kMaxLogLineSize);
    if (!dest) [[unlikely]] {
      return;
    }

    // "INFO ", "ERROR", etc.
    const std::string_view level_str =
        log_level_to_string_with_padding_upper(entry.level);

    // Format ISO8601-like timestamp and level into the buffer
    // Output format: 2026-07-27T14:23:05.123456Z [INFO ] [location] Message\n
    // TODO(logging): Include Location once added to LogEntry:
    // " [{}:{}]", entry.location.file, entry.location.line
    const auto result =
        fmt::format_to_n(reinterpret_cast<char*>(dest), kMaxLogLineSize,
                         FMT_COMPILE("[{}][{}] {}\n"), entry.timestamp_ns,
                         level_str, entry.message);

    writer_.commit_write(result.size);
  }

  void flush() { writer_.finish(); }

 private:
  io::MemoryMappedStreamWriter writer_;
};

static_assert(Sink<FileSink>);

}  // namespace logging
