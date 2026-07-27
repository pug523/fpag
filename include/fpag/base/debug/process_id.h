// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <unistd.h>
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#endif

namespace base {

inline u32 current_process_id() noexcept {
#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  return static_cast<u32>(::getpid());
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return static_cast<u32>(::GetCurrentProcessId());
#else
  return 0;
#endif
}

}  // namespace base

