// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <io.h>
#include <windows.h>
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <unistd.h>
#else
#error "Unsupported platform for SyncLogger"
#endif

namespace base {

#if FPAG_BUILD_FLAG(IS_OS_WIN)
constexpr i32 kStdoutFd = 1;
constexpr i32 kStderrFd = 2;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
constexpr i32 kStdoutFd = STDOUT_FILENO;
constexpr i32 kStderrFd = STDERR_FILENO;
#endif

inline void write(i32 fd, const char* data, size_t size) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  ::_write(fd, data, static_cast<u32>(size));
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  ::write(fd, data, size);
#endif
}

}  // namespace base
