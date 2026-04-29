// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <string>
#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/logging/async/codec/codec.h"

namespace logging {

template <>
struct Codec<std::string_view> {
  using DecodedType = std::string_view;

  inline static usize encode(char* const out, const std::string_view in) {
    const usize arg_size = in.size();
    std::memcpy(out, in.data(), arg_size);
    return arg_size;
  }

  inline static std::string_view decode(const char* data, usize size) {
    return std::string_view{data, size};
  }

  static consteval bool is_fixed_size() { return false; }
};

template <>
struct Codec<std::string> {
  using DecodedType = Codec<std::string_view>::DecodedType;

  inline static usize encode(char* const out, const std::string& in) {
    return Codec<std::string_view>::encode(out, std::string_view{in});
  }

  static std::string_view decode(const char* data, usize size) {
    return Codec<std::string_view>::decode(data, size);
  }

  static consteval bool is_fixed_size() {
    return Codec<std::string_view>::is_fixed_size();
  }
};

template <>
struct Codec<const char*> {
  using DecodedType = Codec<std::string_view>::DecodedType;

  inline static usize encode(char* const out, const char* const in) {
    return Codec<std::string_view>::encode(out, std::string_view{in});
  }

  static std::string_view decode(const char* data, usize size) {
    return Codec<std::string_view>::decode(data, size);
  }

  static consteval bool is_fixed_size() {
    return Codec<std::string_view>::is_fixed_size();
  }
};

}  // namespace logging
