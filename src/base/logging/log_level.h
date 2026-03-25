// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "base/numeric.h"

namespace base {

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
    "\033[1;38;2;064;064;064mtrace: ",  // Trace: Gray
    "\033[1;38;2;040;190;240mdebug: ",  // Debug: Blue
    "\033[1;38;2;050;230;050m info: ",  // Info: Green
    "\033[1;38;2;245;220;015m warn: ",  // Warn: Yellow
    "\033[1;38;2;255;005;005merror: ",  // Error: Red
    "\033[1;38;2;255;040;255mfatal: ",  // Fatal: Magenta
};

inline constexpr std::string_view log_prefix(LogLevel level,
                                             bool ansi = false) {
  return ansi ? kAnsiPrefixes[static_cast<u8>(level)]
              : kPlainPrefixes[static_cast<u8>(level)];
}

}  // namespace base
