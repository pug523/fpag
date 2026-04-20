// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/logging/log_level.h"

namespace logging {

struct LogEntry {
  LogLevel level;
  // 7 B padding
  std::string_view message;
  u64 timestamp_ns;
};

}  // namespace logging
