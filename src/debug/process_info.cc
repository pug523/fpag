// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/numeric.h"
#include "fpag/build/build_flag.h"
#include "fpag/debug/process_id.h"
#include "fpag/debug/thread_id.h"

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <pthread.h>
#include <unistd.h>
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#endif

namespace debug {

u64 current_thread_id() noexcept {
#if FPAG_BUILD_FLAG(IS_OS_APPLE)
  u64 tid = 0;
  ::pthread_threadid_np(nullptr, &tid);
  return tid;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return static_cast<u64>(::pthread_self());
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return static_cast<u64>(::GetCurrentThreadId());
#else
  return 0;
#endif
}

u32 current_process_id() noexcept {
#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  return static_cast<u32>(::getpid());
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return static_cast<u32>(::GetCurrentProcessId());
#else
  return 0;
#endif
}

}  // namespace debug
