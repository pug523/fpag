// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <cstdint>

#include "fpag/build/build_config.h"

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using i128 = __int128_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = __uint128_t;
using usize = size_t;

using f32 = float;
using f64 = double;

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <unistd.h>
using isize = ssize_t;
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
#if FPAG_BUILD_FLAG(ARCH_CPU_64_BITS)
using isize = i64;
#elif FPAG_BUILD_FLAG(ARCH_CPU_32_BITS)
using isize = i32;
#else
#error "Unsupported cpu architecture"
#endif
#else
#error "Unsupported os"
#endif
