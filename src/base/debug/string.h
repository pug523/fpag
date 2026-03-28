// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/numeric.h"

namespace base {

template <typename T>
concept StringViewable = requires(T t) { std::string_view{t}; };

template <typename T>
concept FormattableNumber = std::is_arithmetic_v<std::remove_cvref_t<T>>;

template <typename T>
concept PointerType = std::is_pointer_v<std::remove_cvref_t<T>> ||
                      std::is_null_pointer_v<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr auto to_debug_string(const T& value) {
  if constexpr (StringViewable<T>) {
    return std::string_view{value};
  } else if constexpr (FormattableNumber<T>) {
    return std::to_string(value);
  } else if constexpr (PointerType<T>) {
    if (!value) {
      return std::string("0x0");
    }
    return std::to_string(reinterpret_cast<uintptr_t>(value));
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
