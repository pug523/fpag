// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <string>
#include <string_view>

#include "base/numeric.h"
#include "logging/async/codec/codec.h"

namespace logging {

template <>
struct Codec<std::string> {
  using DecodedType = std::string_view;

  static usize encode(char* const out, const std::string& in) {
    constexpr DecodeFunction<std::string_view> kDecoderPtr = &decode;
    std::memcpy(out, reinterpret_cast<const void*>(kDecoderPtr),
                sizeof(kDecoderPtr));

    const usize arg_size = in.size();
    std::memcpy(out + sizeof(kDecoderPtr), in.data(), arg_size);

    const usize written = sizeof(kDecoderPtr) + arg_size;
    return written;
  }

  static std::string_view decode(const char* data, usize size) {
    return std::string_view{data, size};
  }

  static consteval bool is_fixed_size() { return false; }
};

template <>
struct Codec<const char*> {
  using DecodedType = std::string_view;

  static usize encode(char* const out, const char* const in) {
    constexpr DecodeFunction<std::string_view> kDecoderPtr = &decode;
    std::memcpy(out, reinterpret_cast<const void*>(kDecoderPtr),
                sizeof(kDecoderPtr));

    const usize arg_size = std::strlen(in);
    std::memcpy(out + sizeof(kDecoderPtr), in, arg_size);

    const usize written = sizeof(kDecoderPtr) + arg_size;
    return written;
  }

  static std::string_view decode(const char* data, usize size) {
    return std::string_view{data, size};
  }

  static consteval bool is_fixed_size() { return false; }
};

}  // namespace logging
