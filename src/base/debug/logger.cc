// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/logger.h"

#include "fpag/base/console.h"

namespace base {

DebugLogger debug_logger;

void init_logger() {
  // debug_logger.init(logging::StdoutSink(
  //     static_cast<char*>(mem::allocate_pages(mem::kPageSize)),
  //     mem::kPageSize, base::console_color_style(base::Stream::Stdout),
  //     true));
  debug_logger.init(logging::StdoutSink(
      nullptr, 0, base::console_color_style(base::Stream::Stdout), false));
}

}  // namespace base
