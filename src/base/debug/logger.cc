// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/logger.h"

#include "fpag/base/console.h"
#include "fpag/mem/page_allocator.h"

namespace base {

DebugLogger& debug_logger() {
  static DebugLogger logger = [] {
    DebugLogger l;
    l.init(logging::StdoutSink(
        static_cast<char*>(mem::allocate_pages(mem::kPageSize)), mem::kPageSize,
        base::console_color_mode(base::Stream::Stdout), true));
    return l;
  }();

  return logger;
}

}  // namespace base
