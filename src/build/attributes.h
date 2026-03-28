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

#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
#define FPAG_COLD [[gnu::cold]]
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#define FPAG_COLD
#else
#define FPAG_COLD
#endif

#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
#define FPAG_NOINLINE __attribute__((noinline))
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#define FPAG_NOINLINE __declspec(noinline)
#else
#define FPAG_NOINLINE
#endif

#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
#define FPAG_VISIBLE __attribute__((visibility("default")))
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#define FPAG_VISIBLE __declspec(dllexport)
#else
#define FPAG_VISIBLE
#endif
