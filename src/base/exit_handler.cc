// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/exit_handler.h"

#include <cstdlib>

#include "fpag/base/console.h"
#include "fpag/base/debug/string.h"
#include "fpag/base/io_util.h"
#include "fpag/base/style.h"

namespace base {

namespace {

void reset_console_colors() {
  if (is_ansi_available(Stream::Stdout)) {
    ::base::write(kStdoutFd, kReset, const_strlen(kReset));
  }
  if (is_ansi_available(Stream::Stderr)) {
    ::base::write(kStderrFd, kReset, const_strlen(kReset));
  }
}

void on_exit() {
  reset_console_colors();
}

}  // namespace

void register_exit_handler() {
  std::atexit(on_exit);
}

}  // namespace base
