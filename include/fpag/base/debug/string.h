// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <string_view>
#include <type_traits>

#include "fmt/base.h"
#include "fmt/format.h"
#include "fpag/base/numeric.h"

namespace base {

template <typename T, typename Char = char>
concept Formattable = requires(T& val, fmt::format_context ctx) {
  fmt::formatter<std::remove_cvref_t<T>, Char>().parse(
      std::declval<fmt::basic_format_parse_context<Char>&>());
  fmt::formatter<std::remove_cvref_t<T>, Char>().format(val, ctx);
};

template <typename T>
concept StringViewable = requires(T t) { std::string_view{t}; };

template <typename T>
inline constexpr auto to_debug_string(const T& value) {
  if constexpr (StringViewable<T>) {
    return std::string_view{value};
  } else if constexpr (Formattable<T>) {
    return fmt::format("{}", value);
  } else {
    return std::string_view{"<unprintable>"};
  }
}

inline consteval usize const_strlen(const char* s) {
  usize len = 0;
  while (s[len] != '\0') {
    len++;
  }
  return len;
}

}  // namespace base
