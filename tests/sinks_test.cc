// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fmt/format.h"
#include "fpag/base/numeric.h"
#include "fpag/debug/time_util.h"
#include "fpag/io/file_handle.h"
#include "fpag/io/memory_mapped_file.h"
#include "fpag/io/temp_file.h"
#include "fpag/logging/async/async_logger.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/composite_sink.h"
#include "fpag/logging/sink/file_sink.h"
#include "fpag/logging/sink/json_lines_sink.h"
#include "fpag/logging/sink/null_sink.h"
#include "fpag/logging/sink/sink.h"
#include "fpag/logging/sync/sync_logger.h"

namespace logging {

TEST_CASE("FileSink output validation", "[logging][sink][file_sink]") {
  const io::TempFile temp_file;
  REQUIRE(temp_file.is_valid());

  FileSink sink(temp_file.path());
  const u64 ts = debug::current_timestamp_ns();
  const LogEntry entry1{.level = LogLevel::Info,
                        .message = "FileSink write test",
                        .timestamp_ns = ts};
  sink.log(entry1);
  sink.flush();

  io::FileHandle handle;
  REQUIRE(handle.open(temp_file.path(), io::FileAccess::Read));

  io::MemoryMappedFile mmap;
  REQUIRE(mmap.map(handle, 0, 0));

  const std::string_view content(reinterpret_cast<const char*>(mmap.data()),
                                 mmap.size());
  const std::string formatted_ts = fmt::format("[{}]", ts);
  CHECK(content.find(formatted_ts) != std::string::npos);
  CHECK(content.find("[INFO ]") != std::string::npos);
  CHECK(content.find("FileSink write test\n") != std::string::npos);
}

TEST_CASE("JsonLinesSink output validation",
          "[logging][sink][json_lines_sink]") {
  const io::TempFile temp_file;
  REQUIRE(temp_file.is_valid());

  JsonLinesSink sink(temp_file.path());

  const u64 ts = debug::current_timestamp_ns();
  const LogEntry entry{.level = LogLevel::Error,
                       .message = "Failed to connect to cluster",
                       .timestamp_ns = ts};
  sink.log(entry);
  sink.flush();

  io::FileHandle handle;
  REQUIRE(handle.open(temp_file.path(), io::FileAccess::Read));

  io::MemoryMappedFile mmap;
  REQUIRE(mmap.map(handle, 0, 0));

  const std::string_view content(reinterpret_cast<const char*>(mmap.data()),
                                 mmap.size());
  const std::string formatted_ts = fmt::format("\"ts\":{}", ts);
  CHECK(content.find("\"level\":\"error\"") != std::string::npos);
  CHECK(content.find("\"msg\":\"Failed to connect to cluster\"") !=
        std::string::npos);
  CHECK(content.find(formatted_ts) != std::string::npos);
}

namespace {

// Records what it was handed, so fan-out can be checked without touching the
// filesystem. Not default constructible either, which is exactly the point.
class CountingSink {
 public:
  explicit CountingSink(usize* logged, usize* flushed)
      : logged_(logged), flushed_(flushed) {}

  CountingSink(CountingSink&&) noexcept = default;
  CountingSink& operator=(CountingSink&&) = delete;

  void log(const LogEntry&) { ++*logged_; }
  void flush() { ++*flushed_; }

 private:
  usize* logged_;
  usize* flushed_;
};

static_assert(Sink<CountingSink>);
static_assert(!std::is_default_constructible_v<CountingSink>);
static_assert(!std::is_move_assignable_v<CountingSink>);

}  // namespace

TEST_CASE("CompositeSink fans out to every sink",
          "[logging][sink][composite_sink]") {
  usize first_logged = 0;
  usize first_flushed = 0;
  usize second_logged = 0;
  usize second_flushed = 0;

  CompositeSink<CountingSink, CountingSink> sink{
      CountingSink{&first_logged, &first_flushed},
      CountingSink{&second_logged, &second_flushed}};

  const LogEntry entry{.level = LogLevel::Warn,
                       .message = "composite fan out",
                       .timestamp_ns = debug::current_timestamp_ns()};
  sink.log(entry);
  sink.flush();

  CHECK(first_logged == 1);
  CHECK(second_logged == 1);
  CHECK(first_flushed == 1);
  CHECK(second_flushed == 1);
}

// A sink is handed to a logger through init(), so it never has to be default
// constructible. CompositeSink is the case that proves it: it only has a
// constructor taking the sinks it aggregates.
TEST_CASE("Loggers accept a non-default-constructible sink",
          "[logging][sink][composite_sink]") {
  using Composite = CompositeSink<NullSink, NullSink>;
  static_assert(!std::is_default_constructible_v<Composite>);

  SECTION("sync logger") {
    SyncLogger<Composite, kDefaultLogLevel> logger;
    logger.init(Composite{NullSink{}, NullSink{}});
    logger.info("composite {}", 168);
    logger.flush();

    auto moved = std::move(logger);
    moved.flush();
  }

  SECTION("async logger") {
    AsyncLogger<Composite, LogLevel::Trace> logger;
    logger.init(Composite{NullSink{}, NullSink{}});
    logger.start_backend_worker();
    logger.info("composite {}", 168);
    logger.flush();
    logger.stop_backend_worker();
  }
}

}  // namespace logging
