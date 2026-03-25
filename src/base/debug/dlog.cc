// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/dlog.h"

#include <format>
#include <string_view>

#include "base/console.h"
#include "base/logging/log_level.h"
#include "base/logging/sync_logger.h"
#include "base/mem/page_allocator.h"
#include "base/numeric.h"

namespace base::internal {

namespace {

inline SyncLogger& dlog_instance() {
  static SyncLogger logger(static_cast<char*>(allocate_pages(kPageSize)),
                           kPageSize, LogLevel::Debug,
                           is_ansi_escape_sequence_available(Stream::Stdout));
  return logger;
}

}  // namespace

void dlog_impl(std::string_view formatted_msg,
               const char* file,
               i32 line,
               const char* func) {
  dlog_instance().debug("{}  [on {} ({}:{})]\n", formatted_msg, func, file,
                        line);
}

}  // namespace base::internal
