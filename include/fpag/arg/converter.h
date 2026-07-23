// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <charconv>
#include <concepts>
#include <string>
#include <string_view>
#include <system_error>

#include "fpag/base/numeric.h"
#include "fpag/base/result.h"

namespace arg {

enum class GetError : u8 {
  InvalidArgument,
  OutOfRange,
};

/// Customization point for parsing command-line string values into type @tparam
/// T.
///
/// To add parsing support for a custom type, specialize this struct and
/// implement:
/// @code
/// static base::Result<T, GetError> from_string(std::string_view v);
/// @endcode
///
/// @tparam T Target type to parse from string_view.
template <typename T>
struct Converter;

/// Concept checking if type @tparam T can be parsed from a string via @ref
/// Converter.
///
/// Requires @c Converter<T>::from_string(v) to return a
/// @c base::Result<T, GetError>.
///
/// @tparam T Type to validate for command-line argument parsing capability.
template <typename T>
concept Parsable = requires(std::string_view v) {
  { Converter<T>::from_string(v) } -> std::same_as<base::Result<T, GetError>>;
};

template <>
struct Converter<std::string_view> {
  static base::Result<std::string_view, GetError> from_string(
      std::string_view v) {
    return base::make_ok(v);
  }
};

template <>
struct Converter<std::string> {
  static base::Result<std::string, GetError> from_string(std::string_view v) {
    return base::make_ok(std::string(v));
  }
};

template <>
struct Converter<bool> {
  static base::Result<bool, GetError> from_string(std::string_view v) {
    if (v == "true" || v == "1" || v == "y") {
      return base::make_ok(true);
    }
    if (v == "false" || v == "0" || v == "n") {
      return base::make_ok(false);
    }
    return base::make_err(GetError::InvalidArgument);
  }
};

template <typename N>
  requires((std::is_integral_v<N> && !std::is_same_v<N, bool>) ||
           std::is_floating_point_v<N>)
struct Converter<N> {
  static base::Result<N, GetError> from_string(std::string_view v) {
    N val{};
    auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), val);
    if (ec != std::errc()) {
      return base::make_err(GetError::InvalidArgument);
    }
    return base::make_ok(val);
  }
};

}  // namespace arg
