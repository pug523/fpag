// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <array>
#include <cstddef>

#include "base/numeric.h"

namespace base {

// Format
constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kDim = "\033[2m";
constexpr const char* kItalic = "\033[3m";
constexpr const char* kUnderline = "\033[4m";
constexpr const char* kBlink = "\033[5m";
constexpr const char* kReverse = "\033[7m";
constexpr const char* kHidden = "\033[8m";
constexpr const char* kStrike = "\033[9m";

// Color
constexpr const char* kBlack = "\033[30m";
constexpr const char* kRed = "\033[31m";
constexpr const char* kGreen = "\033[32m";
constexpr const char* kYellow = "\033[33m";
constexpr const char* kBlue = "\033[34m";
constexpr const char* kMagenta = "\033[35m";
constexpr const char* kCyan = "\033[36m";
constexpr const char* kWhite = "\033[37m";
constexpr const char* kGray = "\033[90m";
constexpr const char* kBrightRed = "\033[91m";
constexpr const char* kBrightGreen = "\033[92m";
constexpr const char* kBrightYellow = "\033[93m";
constexpr const char* kBrightBlue = "\033[94m";
constexpr const char* kBrightMagenta = "\033[95m";
constexpr const char* kBrightCyan = "\033[96m";
constexpr const char* kBrightWhite = "\033[97m";

// Background color
constexpr const char* kBgBlack = "\033[40m";
constexpr const char* kBgRed = "\033[41m";
constexpr const char* kBgGreen = "\033[42m";
constexpr const char* kBgYellow = "\033[43m";
constexpr const char* kBgBlue = "\033[44m";
constexpr const char* kBgMagenta = "\033[45m";
constexpr const char* kBgCyan = "\033[46m";
constexpr const char* kBgWhite = "\033[47m";
constexpr const char* kBgGray = "\033[100m";
constexpr const char* kBgBrightRed = "\033[101m";
constexpr const char* kBgBrightGreen = "\033[102m";
constexpr const char* kBgBrightYellow = "\033[103m";
constexpr const char* kBgBrightBlue = "\033[104m";
constexpr const char* kBgBrightMagenta = "\033[105m";
constexpr const char* kBgBrightCyan = "\033[106m";
constexpr const char* kBgBrightWhite = "\033[107m";

// Utility
constexpr const char* kFgRgbPrefix = "\033[38;2;";
constexpr const char* kBgRgbPrefix = "\033[48;2;";
constexpr const char kRgbSuffix = 'm';
constexpr const char kSemicolon = ';';
constexpr const usize kStyleCodeLength = 4;
constexpr const usize kResetCodeLength = kStyleCodeLength;
constexpr const usize kRgbCodeLength = 20;

// Logging
constexpr const char* kDebugStyle = "\033[1;38;2;40;170;245m";  // Bold Cyan
constexpr const char* kInfoStyle = "\033[1;38;2;50;255;50m";    // Bold Green
constexpr const char* kWarnStyle = "\033[1;38;2;255;220;15m";   // Bold Yellow
constexpr const char* kErrorStyle = "\033[1;38;2;255;5;5m";     // Bold Red
constexpr const char* kFatalStyle = "\033[1;38;2;255;40;255m";  // Bold Magenta

}  // namespace base
