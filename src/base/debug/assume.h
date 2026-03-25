// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "build/build_flag.h"

#if FPAG_BUILDFLAG(COMPILER_CLANG)
#define FPAG_ASSUME(expr) __builtin_assume(static_cast<bool>(expr))

#elif FPAG_BUILDFLAG(COMPILER_GCC)
#define FPAG_ASSUME(expr)         \
  do {                            \
    if (!static_cast<bool>(expr)) \
      __builtin_unreachable();    \
  } while (0)

#elif FPAG_BUILDFLAG(COMPILER_MSVC)

#define FPAG_ASSUME(expr) __assume(static_cast<bool>(expr))

#else

#define FPAG_ASSUME(expr)

#endif
