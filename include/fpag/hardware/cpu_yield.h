// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_ARCH_X86_FAMILY)
#include <immintrin.h>
#elif FPAG_BUILD_FLAG(IS_ARCH_ARM_FAMILY)
#else
#include <thread>
#endif

namespace hardware {

inline void cpu_yield() {
#if FPAG_BUILD_FLAG(IS_ARCH_X86_FAMILY)
  _mm_pause();
#elif FPAG_BUILD_FLAG(IS_ARCH_ARM_FAMILY)
  __asm__ volatile("yield" ::: "memory");
#else
  std::this_thread::yield();
#endif
}

}  // namespace hardware
