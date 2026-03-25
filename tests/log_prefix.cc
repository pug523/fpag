// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/log_prefix.h"

#include <string_view>
#include <vector>

#include "catch2/catch_test_macros.hpp"

namespace base {

TEST_CASE("Log prefix consistency", "[base][logging]") {
  // We verify both the generic log_prefix and the specific helper functions.
  // The expected content depends on whether ANSI is enabled or not.

  SECTION("Individual prefix helpers match the main log_prefix function") {
    // Ensure that specific helpers return the same view as the generic
    // function.
    CHECK(debug_prefix() == log_prefix(LogLevel::Debug));
    CHECK(info_prefix() == log_prefix(LogLevel::Info));
    CHECK(warn_prefix() == log_prefix(LogLevel::Warn));
    CHECK(error_prefix() == log_prefix(LogLevel::Error));
    CHECK(fatal_prefix() == log_prefix(LogLevel::Fatal));
  }

  SECTION("Prefixes contain expected labels regardless of ANSI support") {
    // Even if ANSI escape codes are present, the labels should be there.
    CHECK(log_prefix(LogLevel::Debug).find("debug") != std::string_view::npos);
    CHECK(log_prefix(LogLevel::Info).find("info") != std::string_view::npos);
    CHECK(log_prefix(LogLevel::Warn).find("warn") != std::string_view::npos);
    CHECK(log_prefix(LogLevel::Error).find("error") != std::string_view::npos);
    CHECK(log_prefix(LogLevel::Fatal).find("fatal") != std::string_view::npos);
  }
}

TEST_CASE("Log prefix level coverage", "[base][logging]") {
  // Verify all levels up to MaxValue are handled.
  const std::vector<LogLevel> levels = {LogLevel::Debug, LogLevel::Info,
                                        LogLevel::Warn, LogLevel::Error,
                                        LogLevel::Fatal};

  for (const auto level : levels) {
    std::string_view prefix = log_prefix(level);

    CAPTURE(static_cast<int>(level));  // Provides context on failure
    CHECK_FALSE(prefix.empty());
    CHECK(prefix.ends_with(": "));
  }
}

TEST_CASE("Ansi detection behavior", "[base][logging]") {
  std::string_view p = log_prefix(LogLevel::Info);

  if (p.size() == 7) {
    // If it's a plain prefix, it should be exactly 7 characters " info: "
    CHECK(p == " info: ");
  } else {
    // If it's ANSI, it should contain the Escape character \x1b
    CHECK(p.find('\033') != std::string_view::npos);
  }
}

}  // namespace base
