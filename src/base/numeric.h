// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using usize = size_t;

static_assert(std::numeric_limits<float>::is_iec559 && sizeof(float) == 4,
              "f32 requires IEEE754 32 bit float");
static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8,
              "f64 requires IEEE754 64 bit float");
using f32 = float;
using f64 = double;
