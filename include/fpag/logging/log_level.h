// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

namespace logging {

enum class LogLevel : u8 {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warn = 3,
  Error = 4,
  Fatal = 5,

  WithoutPrefix = 6,

  Off = (1 << 8) - 1,
  All = 0,
};

#if FPAG_BUILD_FLAG(IS_DEBUG)
constexpr LogLevel kDefaultLogLevel = LogLevel::Debug;
#else
constexpr LogLevel kDefaultLogLevel = LogLevel::Info;
#endif

}  // namespace logging
