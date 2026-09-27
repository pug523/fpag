// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

namespace mem {

#if FPAG_BUILD_FLAG(IS_ARCH_64_BITS)
constexpr usize CACHE_LINE_SIZE = 64;
#elif FPAG_BUILD_FLAG(IS_ARCH_32_BITS)
constexpr usize CACHE_LINE_SIZE = 32;
#else
#error "Unsupported cpu architecture"
// constexpr usize CACHE_LINE_SIZE = 64;
#endif

}  // namespace mem
