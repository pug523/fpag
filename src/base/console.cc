// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/console.h"

#include "fpag/base/debug/fatal.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace base {

namespace {

bool check_ansi_sequence_available(Stream stream) {
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
  if (stream == Stream::Stdout) {
    return isatty(STDOUT_FILENO);
  } else if (stream == Stream::Stderr) {
    return isatty(STDERR_FILENO);
  }
  FPAG_UNREACHABLE();
#endif
}

}  // namespace

bool is_ansi_escape_sequence_available(Stream stream) {
  static const bool out = check_ansi_sequence_available(Stream::Stdout);
  static const bool err = check_ansi_sequence_available(Stream::Stderr);
  switch (stream) {
    case Stream::Stdout: return out;
    case Stream::Stderr: return err;
    default: FPAG_UNREACHABLE();
  }
}

void register_console() {
  // Set console mode to utf-8
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);
#endif
  // Call these once to initialize the static variable
  is_ansi_escape_sequence_available(Stream::Stdout);
  is_ansi_escape_sequence_available(Stream::Stderr);
}

}  // namespace base
