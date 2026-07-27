// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <cstring>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_CPU_X86_SSE2)
#include <immintrin.h>
#elif FPAG_BUILD_FLAG(IS_CPU_ARM_NEON)
#include <arm_neon.h>
#endif

namespace base {

// 16 B metadata group processing with zero-overhead compile-time dispatch.
class SimdControlGroup {
 public:
  static constexpr usize kGroupSize = 16;

  explicit SimdControlGroup(const u8* ctrl_ptr) {
#if FPAG_BUILD_FLAG(IS_CPU_X86_SSE2)
    ctrl_vec_ = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ctrl_ptr));
#elif FPAG_BUILD_FLAG(IS_CPU_ARM_NEON)
    ctrl_vec_ = vld1q_u8(ctrl_ptr);
#else
    std::memcpy(data_, ctrl_ptr, kGroupSize);
#endif
  }

  // Returns a bitmask where bit `i` is set if byte `i` matches `target_h2`.
  [[nodiscard]] inline u32 match_h2(u8 target_h2) const {
#if FPAG_BUILD_FLAG(IS_CPU_X86_SSE2)
    const __m128i target_vec = _mm_set1_epi8(static_cast<char>(target_h2));
    const __m128i cmp = _mm_cmpeq_epi8(ctrl_vec_, target_vec);
    return static_cast<u32>(_mm_movemask_epi8(cmp));
#elif FPAG_BUILD_FLAG(IS_CPU_ARM_NEON)
    const uint8x16_t target_vec = vdupq_n_u8(target_h2);
    const uint8x16_t cmp = vceqq_u8(ctrl_vec_, target_vec);
    const uint8x16_t masked = vandq_u8(cmp, kNeonBitMask_);
    const uint64x2_t p = vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(masked)));
    const u32 low = static_cast<u32>(vgetq_lane_u64(p, 0));
    const u32 high = static_cast<u32>(vgetq_lane_u64(p, 1));
    return low | (high << 8);
#else
    u32 mask = 0;
    for (usize i = 0; i < kGroupSize; ++i) {
      if (data_[i] == target_h2) {
        mask |= (1U << i);
      }
    }
    return mask;
#endif
  }

  // Returns a bitmask where bit `i` is set if byte `i` indicates an empty slot.
  [[nodiscard]] inline u32 match_empty(u8 empty_marker) const {
    return match_h2(empty_marker);
  }

  // Returns a bitmask where bit `i` is set if byte `i` indicates a locked slot.
  [[nodiscard]] inline u32 match_locked(u8 locked_marker) const {
    return match_h2(locked_marker);
  }

 private:
#if FPAG_BUILD_FLAG(IS_CPU_X86_SSE2)
  __m128i ctrl_vec_;
#elif FPAG_BUILD_FLAG(IS_CPU_ARM_NEON)
  uint8x16_t ctrl_vec_;
  static inline const uint8x16_t kNeonBitMask_ =
      vcombine_u8(vcreate_u8(0x0804020108040201ULL),
                  vcreate_u8(0x8040201080402010ULL));
#else
  u8 data_[kGroupSize];
#endif
};

}  // namespace base

