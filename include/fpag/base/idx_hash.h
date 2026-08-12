// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <functional>

#include "fpag/base/idx.h"
#include "fpag/base/numeric.h"

namespace std {

template <typename T, typename I>
struct hash<base::Idx<T, I>> {
  constexpr usize operator()(const base::Idx<T, I>& id) const noexcept {
    return std::hash<I>{}(id.idx);
  }
};

}  // namespace std

