// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "base/numeric.h"
#include "build/build_config.h"

namespace logging {

enum class LogLevel : u8 {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warn = 3,
  Error = 4,
  Fatal = 5,
  MaxValue = Fatal,
};

static constexpr std::string_view kPlainPrefixes[] = {
    "trace: ",  // Trace
    "debug: ",  // Debug
    " info: ",  // Info
    " warn: ",  // Warn
    "error: ",  // Error
    "fatal: ",  // Fatal
};

static constexpr std::string_view kAnsiPrefixes[] = {
    "\033[1;38;2;064;064;064mtrace\033[0m: ",  // Trace: Gray
    "\033[1;38;2;040;190;240mdebug\033[0m: ",  // Debug: Blue
    "\033[1;38;2;050;230;050m info\033[0m: ",  // Info: Green
    "\033[1;38;2;245;220;015m warn\033[0m: ",  // Warn: Yellow
    "\033[1;38;2;255;005;005merror\033[0m: ",  // Error: Red
    "\033[1;38;2;255;040;255mfatal\033[0m: ",  // Fatal: Magenta
};

inline constexpr std::string_view log_prefix(LogLevel level,
                                             bool ansi = false) {
  return ansi ? kAnsiPrefixes[static_cast<u8>(level)]
              : kPlainPrefixes[static_cast<u8>(level)];
}

#if FPAG_BUILD_FLAG(IS_DEBUG)
constexpr LogLevel kDefaultLogLevel = LogLevel::Debug;
#else
constexpr LogLevel kDefaultLogLevel = LogLevel::Info;
#endif

}  // namespace logging
