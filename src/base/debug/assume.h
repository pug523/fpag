// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
#define FPAG_ASSUME(expr) __builtin_assume(static_cast<bool>(expr))
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#define FPAG_ASSUME(expr) __assume(static_cast<bool>(expr))
#else
#define FPAG_ASSUME(expr)
#endif
