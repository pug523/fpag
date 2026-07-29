// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/logger.h"

#include "fpag/term/console.h"

namespace debug {

DebugLogger debug_logger;

void init_debug_logger() {
  // debug_logger.init(logging::StdoutSink(
  //     static_cast<char*>(mem::allocate_pages(mem::page_size())),
  //     mem::page_size(), term::console_color_style(term::Stream::Stdout),
  //     true));
  debug_logger.init(logging::StdoutSink(
      nullptr, 0, term::console_color_style(term::Stream::Stdout), false));
}

}  // namespace debug
