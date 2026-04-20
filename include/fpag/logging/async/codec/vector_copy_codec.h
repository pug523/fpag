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
    std::memcpy(out, reinterpret_cast<const void*>(kDecoderPtr),
                sizeof(kDecoderPtr));

    const usize arg_size = sizeof(T) * in.size();
    std::memcpy(out + sizeof(kDecoderPtr), in.data(), arg_size);

    const usize written = sizeof(kDecoderPtr) + arg_size;
    return written;
  }

  static std::vector<T, Allocator> decode(const char* data, usize size) {
    std::vector<T, Allocator> result;
    result.resize(size / sizeof(T));
    std::memcpy(result.data(), data + sizeof(void*), size);
    return result;
  }

  static consteval bool is_fixed_size() { return false; }
};

}  // namespace logging
