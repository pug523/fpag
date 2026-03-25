// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <type_traits>

#include "base/debug/check.h"

namespace base {

template <typename N>
inline constexpr bool is_power_of_two(N n) {
  static_assert(std::is_arithmetic_v<N>);
  return n > 0 && (n & (n - 1)) == 0;
}

template <typename N>
inline constexpr N next_power_of_two(N v) {
  static_assert(std::is_arithmetic_v<N>);
  if (v <= 1) {
    return 1;
  }

  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  if constexpr (sizeof(N) > 2) {
    v |= v >> 16;
  }
  if constexpr (sizeof(N) > 4) {
    v |= v >> 32;
  }
  ++v;

  dcheck(is_power_of_two(v));
  return v;
}

}  // namespace base
