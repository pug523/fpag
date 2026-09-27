// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>

#include "fpag/base/numeric.h"
#include "fpag/term/color_style.h"

namespace term {

// Format
constexpr const char* RESET = "\033[0m";
constexpr const char* BOLD = "\033[1m";
constexpr const char* DIM = "\033[2m";
constexpr const char* ITALIC = "\033[3m";
constexpr const char* UNDERLINE = "\033[4m";
constexpr const char* BLINK = "\033[5m";
constexpr const char* REVERSE = "\033[7m";
constexpr const char* HIDDEN = "\033[8m";
constexpr const char* STRIKE = "\033[9m";

// Color
constexpr const char* FG_BLACK = "\033[30m";
constexpr const char* FG_RED = "\033[31m";
constexpr const char* FG_GREEN = "\033[32m";
constexpr const char* FG_YELLOW = "\033[33m";
constexpr const char* FG_BLUE = "\033[34m";
constexpr const char* FG_MAGENTA = "\033[35m";
constexpr const char* FG_CYAN = "\033[36m";
constexpr const char* FG_WHITE = "\033[37m";
constexpr const char* FG_GRAY = "\033[90m";
constexpr const char* FG_BRIGHT_RED = "\033[91m";
constexpr const char* FG_BRIGHT_GREEN = "\033[92m";
constexpr const char* FG_BRIGHT_YELLOW = "\033[93m";
constexpr const char* FG_BRIGHT_BLUE = "\033[94m";
constexpr const char* FG_BRIGHT_MAGENTA = "\033[95m";
constexpr const char* FG_BRIGHT_CYAN = "\033[96m";
constexpr const char* FG_BRIGHT_WHITE = "\033[97m";

// Background color
constexpr const char* BG_BLACK = "\033[40m";
constexpr const char* BG_RED = "\033[41m";
constexpr const char* BG_GREEN = "\033[42m";
constexpr const char* BG_YELLOW = "\033[43m";
constexpr const char* BG_BLUE = "\033[44m";
constexpr const char* BG_MAGENTA = "\033[45m";
constexpr const char* BG_CYAN = "\033[46m";
constexpr const char* BG_WHITE = "\033[47m";
constexpr const char* BG_GRAY = "\033[100m";
constexpr const char* BG_BRIGHT_RED = "\033[101m";
constexpr const char* BG_BRIGHT_GREEN = "\033[102m";
constexpr const char* BG_BRIGHT_YELLOW = "\033[103m";
constexpr const char* BG_BRIGHT_BLUE = "\033[104m";
constexpr const char* BG_BRIGHT_MAGENTA = "\033[105m";
constexpr const char* BG_BRIGHT_CYAN = "\033[106m";
constexpr const char* BG_BRIGHT_WHITE = "\033[107m";

// Utility
constexpr const char* FG_RGB_PREFIX = "\033[38;2;";
constexpr const char* BG_RGB_PREFIX = "\033[48;2;";
constexpr const char RGB_SUFFIX = 'm';
constexpr const char SEMICOLON = ';';
constexpr const usize STYLE_CODE_LENGTH = 4;
constexpr const usize RESET_CODE_LENGTH = STYLE_CODE_LENGTH;
constexpr const usize RGB_CODE_LENGTH = 20;

inline constexpr const char* style_code(const char* code,
                                        ColorStyle mode) noexcept {
  return mode != ColorStyle::Off ? code : "";
}

}  // namespace term
