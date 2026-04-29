// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <vector>

#include "fpag/base/numeric.h"
#include "fpag/logging/async/codec/codec.h"

namespace logging {

template <typename T, typename Allocator>
struct Codec<std::vector<T, Allocator>> {
  using DecodedType = std::vector<T, Allocator>;

  static usize encode(char* const out, const std::vector<T, Allocator>& in) {
    constexpr DecodeFunction<std::vector<T, Allocator>> kDecoderPtr = &decode;
    const usize arg_size = sizeof(T) * in.size();
    std::memcpy(out, in.data(), arg_size);
    return arg_size;
  }

  static std::vector<T, Allocator> decode(const char* data, usize size) {
    std::vector<T, Allocator> result;
    result.resize(size / sizeof(T));
    std::memcpy(result.data(), data, size);
    return result;
  }

  static consteval bool is_fixed_size() { return false; }
};

}  // namespace logging
