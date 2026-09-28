// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/debug/fatal.h"
#include "fpag/logging/log_level.h"
#include "fpag/term/color_style.h"

namespace logging {

inline constexpr std::string_view log_level_to_string(LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return "trace";
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warn: return "warn";
    case LogLevel::Error: return "error";
    case LogLevel::Fatal: return "fatal";
    default: return "none";
  }
}

inline constexpr std::string_view log_level_to_string_upper(LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Fatal: return "FATAL";
    default: return "NONE";
  }
}

inline constexpr std::string_view log_level_to_string_with_padding(
    LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return "trace";
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info ";
    case LogLevel::Warn: return "warn ";
    case LogLevel::Error: return "error";
    case LogLevel::Fatal: return "fatal";
    default: return "none ";
  }
}

inline constexpr std::string_view log_level_to_string_with_padding_upper(
    LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO ";
    case LogLevel::Warn: return "WARN ";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Fatal: return "FATAL";
    default: return "NONE ";
  }
}

static constexpr std::string_view PLAIN_PREFIXES[] = {
    "trace: ",  // Trace
    "debug: ",  // Debug
    " info: ",  // Info
    " warn: ",  // Warn
    "error: ",  // Error
    "fatal: ",  // Fatal
};

// 16 color
static constexpr std::string_view ANSI16_PREFIXES[] = {
    "\033[1;90mtrace\033[0m: ",  // Trace: Bright Black (Gray)
    "\033[1;34mdebug\033[0m: ",  // Debug: Blue
    "\033[1;32m info\033[0m: ",  // Info: Green
    "\033[1;33m warn\033[0m: ",  // Warn: Yellow
    "\033[1;31merror\033[0m: ",  // Error: Red
    "\033[1;35mfatal\033[0m: ",  // Fatal: Magenta
};

// 256 color
static constexpr std::string_view ANSI256_PREFIXES[] = {
    "\033[1;38;5;242mtrace\033[0m: ",  // Trace: Gray
    "\033[1;38;5;39mdebug\033[0m: ",   // Debug: Sky Blue
    "\033[1;38;5;40m info\033[0m: ",   // Info: Green
    "\033[1;38;5;220m warn\033[0m: ",  // Warn: Gold/Yellow
    "\033[1;38;5;196merror\033[0m: ",  // Error: Red
    "\033[1;38;5;201mfatal\033[0m: ",  // Fatal: Pink/Magenta
};

// True color
static constexpr std::string_view ANSI_TRUE_COLOR_PREFIXES[] = {
    "\033[1;38;2;100;100;100mtrace\033[0m: ",  // Trace: Gray
    "\033[1;38;2;140;210;230mdebug\033[0m: ",  // Debug: Sky Blue
    "\033[1;38;2;150;230;140m info\033[0m: ",  // Info: Green
    "\033[1;38;2;230;200;100m warn\033[0m: ",  // Warn: Yellow
    "\033[1;38;2;230;110;120merror\033[0m: ",  // Error: Red
    "\033[1;38;2;180;120;230mfatal\033[0m: ",  // Fatal: Magenta
};

inline constexpr std::string_view log_prefix(LogLevel level,
                                             term::ColorStyle style) {
  if (level >= LogLevel::WithoutPrefix) {
    return "";
  }
  const u8 l = static_cast<u8>(level);
  switch (style) {
    using C = term::ColorStyle;
    case C::Off: return PLAIN_PREFIXES[l];
    case C::Ansi16: return ANSI16_PREFIXES[l];
    case C::Ansi256: return ANSI256_PREFIXES[l];
    case C::AnsiTrueColor: return ANSI_TRUE_COLOR_PREFIXES[l];
    default: FPAG_UNREACHABLE();
  }
}

}  // namespace logging
