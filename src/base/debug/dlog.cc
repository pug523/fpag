// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/dlog.h"

#include <string_view>

#include "base/console.h"
#include "base/numeric.h"
#include "logging/log_level.h"
#include "logging/sync_logger.h"
#include "mem/page_allocator.h"

namespace base::internal {

void dlog_impl(std::string_view formatted_msg,
               const char* file,
               i32 line,
               const char* func) {
  logging::global_sync_logger().debug("{}   [on {} ({}:{})]", formatted_msg,
                                      func, file, line);
}

}  // namespace base::internal
