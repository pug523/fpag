// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <algorithm>
#include <cstring>
#include <type_traits>

#include "fpag/base/numeric.h"
#include "fpag/logging/async/codec/codec.h"

namespace logging {

template <typename T>
struct Codec<T, std::enable_if_t<std::is_trivially_copyable_v<T>>> {
  using DecodedType = T;

  static usize encode(char* const out, const T& in) {
    constexpr DecodeFunction<T> kDecoderPtr = &decode;
    std::memcpy(out, reinterpret_cast<const void*>(kDecoderPtr),
                sizeof(kDecoderPtr));

    constexpr usize kArgSize = sizeof(T);
    std::memcpy(out + sizeof(kDecoderPtr), &in, kArgSize);

    constexpr usize kWritten = sizeof(kDecoderPtr) + kArgSize;
    return kWritten;
  }

  static T decode(const char* data, usize size) {
    T result;
    std::memcpy(&result, data + sizeof(void*), size);
    return result;
  }

  static consteval bool is_fixed_size() { return true; }
  static consteval usize serialized_size() { return sizeof(T); }
};

}  // namespace logging
