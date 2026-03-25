// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/dlog.h"

#include <format>
#include <iostream>
#include <string>
#include <string_view>

#include "base/log_prefix.h"

namespace base::internal {

void dlog_impl(const char* file,
               i32 line,
               const char* func,
               std::string_view fmt,
               std::format_args args) {
  std::clog << std::format("{}{}  [on {} ({}:{})]\n", debug_prefix(),
                           std::vformat(fmt, args), func, file, line);
}

}  // namespace base::internal
