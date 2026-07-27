// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/profiler/time_trace_formatter.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/location.h"
#include "fpag/base/profiler/profile_event.h"

namespace base {

TEST_CASE("TimeTraceFormatter file output", "[base][profiler][formatter]") {
  const char* kTestFilename = "test_chrome_trace.json";

  SECTION("Writes valid JSON structure with events") {
    std::vector<ProfileEvent> events = {
        {
            .name = "main_pass",
            .category = "pipeline",
            .location =
                Location{.file = "main.cc", .function = "main", .line = 10},
            .start_time_ns = 1000000,  // 1000 us
            .duration_ns = 2500000,    // 2500 us
            .thread_id = 1234,
            .process_id = 5678,
        },
        {
            .name = "parse_pass",
            .category = "parser",
            .location =
                Location{.file = "parser.cc", .function = "parse", .line = 42},
            .start_time_ns = 1500000,
            .duration_ns = 500000,
            .thread_id = 1234,
            .process_id = 5678,
        }};

    const bool success =
        TimeTraceFormatter::write_to_file(kTestFilename, events);
    REQUIRE(success);

    // Read back the file content to verify formatting
    std::ifstream ifs(kTestFilename);
    REQUIRE(ifs.is_open());

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    const std::string content = buffer.str();
    ifs.close();

    // Check JSON keys and structural markers
    CHECK(content.find("{\"traceEvents\":[") != std::string::npos);
    CHECK(content.find("\"name\":\"main_pass\"") != std::string::npos);
    CHECK(content.find("\"cat\":\"pipeline\"") != std::string::npos);
    CHECK(content.find("\"ph\":\"X\"") != std::string::npos);
    CHECK(content.find("\"ts\":1000.000") != std::string::npos);
    CHECK(content.find("\"dur\":2500.000") != std::string::npos);
    CHECK(content.find("\"pid\":5678") != std::string::npos);
    CHECK(content.find("\"tid\":1234") != std::string::npos);
    CHECK(content.find("{\"file\":\"main.cc\",\"line\":10}") !=
          std::string::npos);
    CHECK(content.find("]}") != std::string::npos);

    // Clean up temporary file
    std::remove(kTestFilename);
  }

  SECTION("Handles empty event list gracefully") {
    std::vector<ProfileEvent> empty_events;
    const bool success =
        TimeTraceFormatter::write_to_file(kTestFilename, empty_events);
    REQUIRE(success);

    std::ifstream ifs(kTestFilename);
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    const std::string content = buffer.str();
    ifs.close();

    CHECK(content.find("{\"traceEvents\":[") != std::string::npos);
    CHECK(content.find("]}") != std::string::npos);

    std::remove(kTestFilename);
  }
}

}  // namespace base
