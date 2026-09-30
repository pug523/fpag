// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/build/build_config.h"

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

// For a function whose call is on a hot path and whose body the compiler's own
// size heuristic would put behind that call anyway. The argument setup and the
// prologue of the call can cost more than the body.
#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
#define FPAG_ALWAYS_INLINE inline __attribute__((always_inline))
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#define FPAG_ALWAYS_INLINE __forceinline
#else
#define FPAG_ALWAYS_INLINE inline
#endif

#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
#define FPAG_VISIBLE __attribute__((visibility("default")))
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#define FPAG_VISIBLE __declspec(dllexport)
#else
#define FPAG_VISIBLE
#endif

#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
#define FPAG_EMPTY_MEMBER [[no_unique_address]]
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#define FPAG_EMPTY_MEMBER [[msvc::no_unique_address]]
#else
#define FPAG_EMPTY_MEMBER
#endif
