// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "base/numeric.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_LINUX)
#include <ctime>
#else
#include <chrono>
#endif

namespace base {

inline u64 current_timestamp_ns() noexcept {
#if FPAG_BUILD_FLAG(IS_OS_LINUX)
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return static_cast<u64>(ts.tv_sec) * 1000000000ull +
         static_cast<usize>(ts.tv_nsec);
#else
  return static_cast<u64>(
      std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

}  // namespace base
