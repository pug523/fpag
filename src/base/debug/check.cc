// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/check.h"

#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <format>
#include <iostream>
#include <string_view>

#include "base/debug/fatal.h"
#include "base/log_prefix.h"

namespace base::internal {

namespace {

inline constexpr std::size_t const_strlen(const char* s) {
  std::size_t len = 0;
  while (s[len] != '\0') {
    len++;
  }
  return len;
}

}  // namespace

void check_fail_log_and_crash(std::string_view header,
                              std::string_view msg,
                              std::string_view location) {
  std::cerr << fatal_prefix() << header;
  if (!msg.empty()) {
    std::cerr << msg << '\n';
  }
  std::cerr << location;
  fatal_crash_impl();
}

void check_fail_impl(const char* expr,
                     const char* file,
                     i32 line,
                     const char* func,
                     std::string_view msg) {
  check_fail_log_and_crash(std::format("Check failed!\nExpected: '{}'\n", expr),
                           msg,
                           std::format("  at {}:{} ({})\n", file, line, func));
}

void raw_check_fail_impl(const char* expr,
                         const char* file,
                         i32 line,
                         const char* func,
                         std::string_view msg) {
  write(2, "fatal: RAW CHECK FAILED for '",
        const_strlen("fatal: RAW CHECK FAILED for '"));
  write(2, expr, std::strlen(expr));
  write(2, "'\n", const_strlen("'\n"));

  if (!msg.empty()) {
    write(2, msg.data(), msg.size());
    write(2, "\n", const_strlen("\n"));
  }

  write(2, " at ", const_strlen(" at "));
  write(2, file, std::strlen(file));
  write(2, ":", 1);

  char line_buf[32];
  std::snprintf(line_buf, sizeof(line_buf), "%d", line);
  write(2, line_buf, std::strlen(line_buf));

  write(2, " (", const_strlen(" ("));
  write(2, func, std::strlen(func));
  write(2, ")\n", const_strlen(")\n"));

  fatal_crash_impl();
}

}  // namespace base::internal
