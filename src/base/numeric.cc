// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/numeric.h"

#include <limits>

namespace base {

static_assert(std::numeric_limits<f32>::is_iec559 && sizeof(f32) == 4,
              "f32 requires IEEE754 32 bit float");
static_assert(std::numeric_limits<f64>::is_iec559 && sizeof(f64) == 8,
              "f64 requires IEEE754 64 bit float");

}  // namespace base
