// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/dlog.h"

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/logging/sync_logger.h"

namespace base::internal {

void dlog_impl(std::string_view formatted_msg,
               const char* file,
               i32 line,
               const char* func) {
  logging::SyncLogger& l = logging::global_sync_logger();
  l.debug("{}   [on {} ({}:{})]", formatted_msg, func, file, line);
  l.flush();
}

}  // namespace base::internal
