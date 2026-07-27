// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include <string>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fmt/format.h"
#include "fpag/base/file_handle.h"
#include "fpag/base/memory_mapped_file.h"
#include "fpag/base/numeric.h"
#include "fpag/base/temp_file.h"
#include "fpag/base/time_util.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/file_sink.h"
#include "fpag/logging/sink/json_lines_sink.h"

namespace logging {

TEST_CASE("FileSink output validation", "[logging][sink][file_sink]") {
  base::TempFile const temp_file;
  REQUIRE(temp_file.is_valid());

  FileSink sink(temp_file.path());
  const u64 ts = base::current_timestamp_ns();
  LogEntry const entry1{.level = LogLevel::Info,
                  .message = "FileSink write test",
                  .timestamp_ns = ts};
  sink.log(entry1);
  sink.flush();

  base::FileHandle handle;
  REQUIRE(handle.open(temp_file.path(), base::FileAccess::Read));

  base::MemoryMappedFile mmap;
  REQUIRE(mmap.map(handle, 0, 0));

  std::string_view const content(reinterpret_cast<const char*>(mmap.data()),
                           mmap.size());
  std::string const formatted_ts = fmt::format("[{}]", ts);
  CHECK(content.find(formatted_ts) != std::string::npos);
  CHECK(content.find("[INFO ]") != std::string::npos);
  CHECK(content.find("FileSink write test\n") != std::string::npos);
}

TEST_CASE("JsonLinesSink output validation",
          "[logging][sink][json_lines_sink]") {
  base::TempFile const temp_file;
  REQUIRE(temp_file.is_valid());

  JsonLinesSink sink(temp_file.path());

  const u64 ts = base::current_timestamp_ns();
  LogEntry const entry{.level = LogLevel::Error,
                 .message = "Failed to connect to cluster",
                 .timestamp_ns = ts};
  sink.log(entry);
  sink.flush();

  base::FileHandle handle;
  REQUIRE(handle.open(temp_file.path(), base::FileAccess::Read));

  base::MemoryMappedFile mmap;
  REQUIRE(mmap.map(handle, 0, 0));

  std::string_view const content(reinterpret_cast<const char*>(mmap.data()),
                           mmap.size());
  std::string const formatted_ts = fmt::format("\"ts\":{}", ts);
  CHECK(content.find("\"level\":\"error\"") != std::string::npos);
  CHECK(content.find("\"msg\":\"Failed to connect to cluster\"") !=
        std::string::npos);
  CHECK(content.find(formatted_ts) != std::string::npos);
}

}  // namespace logging
