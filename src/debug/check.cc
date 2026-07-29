// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/check.h"

#include <cstring>
#include <string_view>

#include "fmt/base.h"
#include "fmt/compile.h"
#include "fpag/base/numeric.h"
#include "fpag/debug/fatal.h"
#include "fpag/debug/logger.h"
#include "fpag/debug/stack_trace/stack_trace.h"
#include "fpag/debug/string.h"
#include "fpag/io/io_util.h"

namespace debug::internal {

void check_fail_impl(const char* expr,
                     const char* file,
                     i32 line,
                     const char* func,
                     std::string_view msg) {
  DebugLogger& logger = debug_logger;
  if (msg.empty()) {
    logger.fatal(FMT_COMPILE("Check failed!\nExpected: '{}'\n  at {}:{} ({})"),
                 expr, file, line, func);
  } else {
    logger.fatal(
        FMT_COMPILE("Check failed!\nExpected: '{}'\n  at {}:{} ({})\n{}"), expr,
        file, line, func, msg);
  }
  print_stack_trace_from_here();
  logger.flush();

  fatal_crash_impl();
}

void check_op_fail_impl(const char* expected,
                        const std::string_view lhs,
                        const std::string_view rhs,
                        const char* file,
                        i32 line,
                        const char* func,
                        std::string_view msg) {
  DebugLogger& logger = debug_logger;
  if (msg.empty()) {
    logger.fatal(
        FMT_COMPILE(
            "Check failed!\nExpected: '{}', Actual: {} vs {}\n  at {}:{} ({})"),
        expected, lhs, rhs, file, line, func);

  } else {
    logger.fatal(FMT_COMPILE("Check failed!\nExpected: '{}', Actual: {} vs "
                             "{}\n  at {}:{} ({})\n{}"),
                 expected, lhs, rhs, file, line, func, msg);
  }
  print_stack_trace_from_here();
  logger.flush();

  fatal_crash_impl();
}

void raw_check_fail_impl(const char* expr,
                         const char* file,
                         i32 line,
                         const char* func,
                         std::string_view msg) {
  constexpr const char* kHeaderPrefix = "fatal: RAW CHECK FAILED for '";
  constexpr const char* kHeaderSuffix = "'\n";
  constexpr const char* kAt = " at ";
  constexpr const char* kColon = " : ";
  constexpr usize kLineBufSize = 64;
  constexpr const char* kFuncPrefix = " (";
  constexpr const char* kFuncSuffix = ")\n";
  constexpr const char* kNewline = "\n";

  io::write(io::kStderrFd, kHeaderPrefix, const_strlen(kHeaderPrefix));
  io::write(io::kStderrFd, expr, std::strlen(expr));
  io::write(io::kStderrFd, kHeaderSuffix, const_strlen(kHeaderSuffix));

  io::write(io::kStderrFd, kAt, const_strlen(kAt));
  io::write(io::kStderrFd, file, std::strlen(file));
  io::write(io::kStderrFd, kColon, const_strlen(kColon));

  char line_buf[kLineBufSize];
  auto result = fmt::format_to_n(line_buf, sizeof(line_buf), "{}", line);
  io::write(io::kStderrFd, line_buf, result.size);

  io::write(io::kStderrFd, kFuncPrefix, const_strlen(kFuncPrefix));
  io::write(io::kStderrFd, func, std::strlen(func));
  io::write(io::kStderrFd, kFuncSuffix, const_strlen(kFuncSuffix));

  if (!msg.empty()) {
    io::write(io::kStderrFd, msg.data(), msg.size());
    io::write(io::kStderrFd, kNewline, const_strlen(kNewline));
  }

  fatal_crash_impl();
}

}  // namespace debug::internal
