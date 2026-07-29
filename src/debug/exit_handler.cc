// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/exit_handler.h"

#include <cstdlib>

#include "fpag/debug/string.h"
#include "fpag/io/io_util.h"
#include "fpag/term/console.h"
#include "fpag/term/style.h"

namespace debug {

namespace {

void reset_console_colors() {
  if (is_ansi_available(term::Stream::Stdout)) {
    io::write(io::kStdoutFd, term::kReset, const_strlen(term::kReset));
  }
  if (is_ansi_available(term::Stream::Stderr)) {
    io::write(io::kStderrFd, term::kReset, const_strlen(term::kReset));
  }
}

void on_exit() {
  reset_console_colors();
}

}  // namespace

void register_exit_handler() {
  std::atexit(on_exit);
}

}  // namespace debug
