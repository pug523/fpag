// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <limits>
#include <type_traits>

#include "fpag/base/numeric.h"

namespace base {

using IdxBaseType = u32;

constexpr IdxBaseType kInvalidIdx = std::numeric_limits<IdxBaseType>::max();

template <typename T>
concept HasIdxType = requires { typename T::IdxType; };

template <typename T, typename U>
  requires(std::is_integral_v<U>)
struct Idx {
  using IdxType = U;
  // static_assert(std::is_integral_v<IdxType>);

  IdxType idx;

  constexpr explicit Idx(IdxType id) : idx(id) {}
  constexpr auto operator<=>(const Idx&) const = default;

  // Preincrement
  constexpr Idx& operator++() {
    ++idx;
    return *this;
  }

  friend constexpr Idx operator+(const Idx<T, U>& lhs, const Idx<T, U>& rhs) {
    return Idx(lhs.idx + rhs.idx);
  }
  friend constexpr Idx operator-(const Idx<T, U>& lhs, const Idx<T, U>& rhs) {
    return Idx<T, U>(lhs.idx - rhs.idx);
  }

  template <typename N>
  friend constexpr Idx operator+(const Idx& lhs, N i)
    requires(std::is_integral_v<N>)
  {
    return lhs + Idx{static_cast<IdxType>(i)};
  }
  template <typename N>
  friend constexpr Idx operator-(const Idx& lhs, N i)
    requires(std::is_integral_v<N>)
  {
    return lhs - Idx{static_cast<IdxType>(i)};
  }

  bool is_valid() const { return idx != kInvalidIdx; }
};

}  // namespace base

