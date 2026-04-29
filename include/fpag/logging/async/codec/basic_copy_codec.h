// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <type_traits>

#include "fpag/base/numeric.h"
#include "fpag/logging/async/codec/codec.h"

namespace logging {

template <typename T>
struct Codec<T, std::enable_if_t<std::is_trivially_copyable_v<T>>> {
  using DecodedType = T;

  inline static usize encode(char* const out, const T& in) {
    std::memcpy(out, &in, kArgSize);
    return body_size();
  }

  static T decode(const char* data, usize size) {
    T result;
    std::memcpy(&result, data, size);
    return result;
  }

  static constexpr usize kArgSize = sizeof(T);

  static consteval bool is_fixed_size() { return true; }
  static consteval usize body_size() { return kArgSize; }
};

}  // namespace logging
