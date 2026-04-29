// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/dlog.h"

#include <string_view>

#include "fmt/compile.h"
#include "fpag/base/debug/logger.h"
#include "fpag/base/numeric.h"

namespace base::internal {

void dlog_impl(std::string_view formatted_msg,
               const char* file,
               i32 line,
               const char* func) {
  DebugLogger& logger = debug_logger();
  logger.debug(FMT_COMPILE("{}   [on {} ({}:{})]"), formatted_msg, func, file,
               line);
  logger.flush();
}

}  // namespace base::internal
