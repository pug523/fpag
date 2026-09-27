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
  if (!logger.has_sink()) [[unlikely]] {
    // A check can fail before the program hands the debug logger its sink.
    // Logging the failure here would re-enter this function through the
    // logger's own precondition check and overflow the stack, so the report
    // goes straight to stderr.
    raw_check_fail_impl(expr, file, line, func, msg);
  }
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
  if (!logger.has_sink()) [[unlikely]] {
    // The raw report carries the expression and the message, not the operands.
    raw_check_fail_impl(expected, file, line, func, msg);
  }
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
  constexpr const char* HEADER_PREFIX = "fatal: RAW CHECK FAILED for '";
  constexpr const char* HEADER_SUFFIX = "'\n";
  constexpr const char* AT = " at ";
  constexpr const char* COLON = " : ";
  constexpr usize LINE_BUF_SIZE = 64;
  constexpr const char* FUNC_PREFIX = " (";
  constexpr const char* FUNC_SUFFIX = ")\n";
  constexpr const char* NEWLINE = "\n";

  io::write(io::STDERR_FD, HEADER_PREFIX, const_strlen(HEADER_PREFIX));
  io::write(io::STDERR_FD, expr, std::strlen(expr));
  io::write(io::STDERR_FD, HEADER_SUFFIX, const_strlen(HEADER_SUFFIX));

  io::write(io::STDERR_FD, AT, const_strlen(AT));
  io::write(io::STDERR_FD, file, std::strlen(file));
  io::write(io::STDERR_FD, COLON, const_strlen(COLON));

  char line_buf[LINE_BUF_SIZE];
  auto result = fmt::format_to_n(line_buf, sizeof(line_buf), "{}", line);
  io::write(io::STDERR_FD, line_buf, result.size);

  io::write(io::STDERR_FD, FUNC_PREFIX, const_strlen(FUNC_PREFIX));
  io::write(io::STDERR_FD, func, std::strlen(func));
  io::write(io::STDERR_FD, FUNC_SUFFIX, const_strlen(FUNC_SUFFIX));

  if (!msg.empty()) {
    io::write(io::STDERR_FD, msg.data(), msg.size());
    io::write(io::STDERR_FD, NEWLINE, const_strlen(NEWLINE));
  }

  fatal_crash_impl();
}

}  // namespace debug::internal
