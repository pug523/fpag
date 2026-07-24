// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#define _CRT_SECURE_NO_WARNINGS
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cstdlib>
#include <string_view>

#include "fpag/base/color_mode.h"
#include "fpag/base/color_style.h"
#include "fpag/base/console.h"
#include "fpag/base/debug/fatal.h"

namespace base {

namespace {

bool check_ansi_sequence_availability(Stream stream) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  // Check if GetStdHandle() returns terminal handle
  DWORD handle = 0;
  if (stream == Stream::Stdout) {
    handle = STD_OUTPUT_HANDLE;
  } else if (stream == Stream::Stderr) {
    handle = STD_ERROR_HANDLE;
  }
  const HANDLE h_out = GetStdHandle(handle);
  if (h_out == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD dw_mode = 0;
  if (!GetConsoleMode(h_out, &dw_mode)) {
    return false;
  }

  // Enable virtual terminal processing
  dw_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(h_out, dw_mode)) {
    // Cannot use ansi sequence if fail to enable virtual terminal
    return false;
  }
  return true;
#else
  switch (stream) {
    case Stream::Stdout: return isatty(STDOUT_FILENO) != 0;
    case Stream::Stderr: return isatty(STDERR_FILENO) != 0;
    default: FPAG_UNREACHABLE();
  }
#endif
}

ColorStyle console_color_style_internal(Stream stream, ColorMode color_mode) {
  if (color_mode == ColorMode::Never) {
    return ColorStyle::Off;
  } else if (color_mode != ColorMode::Always &&
             (!is_ansi_available(stream) || std::getenv("NO_COLOR"))) {
    return ColorStyle::Off;
  }

  const char* const color_term_env = std::getenv("COLORTERM");
  if (color_term_env) {
    const std::string_view color_term{color_term_env};
    if (color_term == "truecolor" || color_term == "24bit") {
      return ColorStyle::AnsiTrueColor;
    }
    if (color_term == "256color") {
      return ColorStyle::Ansi256;
    }
  }

  const char* const term_env = std::getenv("TERM");
  if (term_env) {
    const std::string_view term{term_env};
    if (term.find("256color") != std::string_view::npos) {
      return ColorStyle::Ansi256;
    }
  }
  return ColorStyle::Ansi16;
}

}  // namespace

bool is_ansi_available(Stream stream) {
  static const bool out = check_ansi_sequence_availability(Stream::Stdout);
  static const bool err = check_ansi_sequence_availability(Stream::Stderr);
  switch (stream) {
    case Stream::Stdout: return out;
    case Stream::Stderr: return err;
    default: FPAG_UNREACHABLE();
  }
}

ColorStyle console_color_style(Stream stream, ColorMode color_mode) {
  switch (stream) {
    case Stream::Stdout:
      return console_color_style_internal(Stream::Stdout, color_mode);
    case Stream::Stderr:
      return console_color_style_internal(Stream::Stderr, color_mode);
    default: FPAG_UNREACHABLE();
  }
}

void register_console() {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  // Set console mode to utf-8
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);
#endif
  // Call these once to initialize the static variable
  console_color_style(Stream::Stdout);
  console_color_style(Stream::Stderr);
}

}  // namespace base
