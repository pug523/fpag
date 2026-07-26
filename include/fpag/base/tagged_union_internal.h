// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

#include "fpag/base/numeric.h"

namespace base::internal {

// Helper trait to find the index of a type in a variadic type list.
template <typename Target, typename... Ts>
struct TypeIndex;

template <typename Target, typename Head, typename... Tail>
struct TypeIndex<Target, Head, Tail...> {
  static constexpr usize value =
      std::is_same_v<Target, Head> ? 0 : 1 + TypeIndex<Target, Tail...>::value;
};

template <typename Target>
struct TypeIndex<Target> {
  static constexpr usize value = 0;
};

// Trait to check if a type exists in a variadic list.
template <typename Target, typename... Ts>
struct ContainsType
    : std::disjunction<std::is_same<std::decay_t<Target>, Ts>...> {};

// Destructor helper for tagged storage.
template <usize Index, typename... Ts>
struct UnionDestructor;

template <usize Index>
struct UnionDestructor<Index> {
  static void destroy(usize /*tag*/, void* /*storage*/) noexcept {
    // noop
  }
};

template <usize Index, typename Head, typename... Tail>
struct UnionDestructor<Index, Head, Tail...> {
  static void destroy(usize tag, void* storage) noexcept {
    if (tag == Index) {
      reinterpret_cast<Head*>(storage)->~Head();
    } else {
      UnionDestructor<Index + 1, Tail...>::destroy(tag, storage);
    }
  }
};

// Move constructor helper for tagged storage.
template <usize Index, typename... Ts>
struct UnionMove;

template <usize Index>
struct UnionMove<Index> {
  static void move_construct(usize /*tag*/,
                             void* /*src*/,
                             void* /*dst*/) noexcept {
    // noop
  }
};

template <usize Index, typename Head, typename... Tail>
struct UnionMove<Index, Head, Tail...> {
  static void move_construct(usize tag, void* src, void* dst) noexcept {
    if (tag == Index) {
      ::new (dst) Head(std::move(*reinterpret_cast<Head*>(src)));
    } else {
      UnionMove<Index + 1, Tail...>::move_construct(tag, src, dst);
    }
  }
};

// Helper detection idiom for 'kCount' or 'Count' member in enum types.
template <typename E, typename = void>
struct HasCountMember : std::false_type {};

template <typename E>
struct HasCountMember<E, std::void_t<decltype(E::kCount)>> : std::true_type {};

template <typename E, typename = void>
struct HasAltCountMember : std::false_type {};

template <typename E>
struct HasAltCountMember<E, std::void_t<decltype(E::Count)>> : std::true_type {
};

// Validates enum type constraints at compile-time.
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

