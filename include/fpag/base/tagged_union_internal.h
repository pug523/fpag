// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "fpag/base/numeric.h"

namespace base::internal {

// Finds the 0-based index of Target within Ts...
template <typename Target, typename... Ts>
struct TypeIndex {
 private:
  template <usize... Is>
  static constexpr usize find_index(std::index_sequence<Is...>) noexcept {
    usize result = sizeof...(Ts);
    bool _ =
        ((std::is_same_v<Target, Ts> ? (result = Is, true) : false) || ...);
    return result;
  }

 public:
  static constexpr usize value = find_index(std::index_sequence_for<Ts...>{});
  static_assert(value < sizeof...(Ts), "Target type was not found in Ts...");
};

// Checks if Target exists within Ts...
template <typename Target, typename... Ts>
struct ContainsType
    : std::disjunction<std::is_same<std::decay_t<Target>, Ts>...> {};

// Helper detection idiom for 'kCount' member in enum types.
template <typename E, typename = void>
struct HasCountMember : std::false_type {};

template <typename E>
struct HasCountMember<E, std::void_t<decltype(E::kCount)>> : std::true_type {};

// Helper detection idiom for 'Count' member in enum types.
template <typename E, typename = void>
struct HasAltCountMember : std::false_type {};

template <typename E>
struct HasAltCountMember<E, std::void_t<decltype(E::Count)>> : std::true_type {
};

// Validates enum type constraints at compile time.
template <typename TagEnum, usize ExpectedCount>
constexpr bool validate_tag_enum() noexcept {
  static_assert(std::is_enum_v<TagEnum>, "TagEnum must be an enum type.");
  static_assert(std::is_integral_v<std::underlying_type_t<TagEnum>>,
                "Underlying type of TagEnum must be an integral type.");

  if constexpr (HasCountMember<TagEnum>::value) {
    static_assert(static_cast<usize>(TagEnum::kCount) == ExpectedCount,
                  "TagEnum::kCount does not match the number of types.");
  } else if constexpr (HasAltCountMember<TagEnum>::value) {
    static_assert(static_cast<usize>(TagEnum::Count) == ExpectedCount,
                  "TagEnum::Count does not match the number of types.");
  }

  return true;
}

// Selects the minimum unsigned integer type that can hold Count values.
template <usize Count>
struct TagTypeImpl {
  using type = std::conditional_t<
      (Count <= std::numeric_limits<u8>::max()),
      u8,
      std::conditional_t<
          (Count <= std::numeric_limits<u16>::max()),
          u16,
          std::conditional_t<(Count <= std::numeric_limits<u32>::max()),
                             u32,
                             usize>>>;
};

template <usize Count>
using TagType = typename TagTypeImpl<Count>::type;

// Default TagEnum generator when none is provided explicitly.
template <typename... Ts>
struct DefaultTagEnum {
  enum class Type : TagType<sizeof...(Ts)>{};
};

template <typename T>
struct TagUnderlyingType {
  using type =
      std::conditional_t<std::is_enum_v<T>, std::underlying_type_t<T>, T>;
};

template <typename T>
using TagUnderlyingType_t = typename TagUnderlyingType<T>::type;

}  // namespace base::internal
