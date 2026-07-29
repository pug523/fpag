// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <pthread.h>
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#endif

namespace debug {

inline u64 current_thread_id() noexcept {
#if FPAG_BUILD_FLAG(IS_OS_APPLE)
  u64 tid;
  pthread_threadid_np(nullptr, &tid);
  return tid;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return static_cast<u64>(::pthread_self());
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return static_cast<u64>(::GetCurrentThreadId());
#else
  return 0;
#endif
}

}  // namespace debug

