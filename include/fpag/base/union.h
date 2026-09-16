// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <type_traits>

#include "fpag/base/numeric.h"

namespace base {

// Untagged union storage with type-safe accessors.
//
// Unlike TaggedUnion, this class stores no tag of its own. It is intended
// for cases where the tag already lives next to the storage (e.g. a struct
// holding both a tag byte and this union), so that adding another tag byte
// would only grow the enclosing struct. Tag checking is the caller's
// responsibility, typically via checked accessor wrappers that verify the
// external tag before calling get().
//
// All member types must be trivially copyable. Reads and writes go through
// std::bit_cast, which keeps every operation constexpr-capable and compiles
// down to plain loads and stores.
template <typename... Ts>
class Union {
 public:
  static_assert(sizeof...(Ts) > 0, "Union requires at least one type.");
  static_assert(!(std::is_void_v<Ts> || ...),
                "Union does not support void members.");
  static_assert(!(std::is_empty_v<Ts> || ...),
                "Union does not support empty members.");
  static_assert((std::is_trivially_copyable_v<Ts> && ...),
                "Union members must be trivially copyable.");

  template <typename T>
    requires((std::is_same_v<T, Ts> || ...))
  constexpr void set(const T& value) noexcept {
    const Bytes<T> bytes = std::bit_cast<Bytes<T>>(value);
    for (usize i = 0; i < sizeof(T); ++i) {
      storage_[i] = bytes[i];
    }
  }

  template <typename T>
    requires((std::is_same_v<T, Ts> || ...))
  constexpr T get() const noexcept {
    Bytes<T> bytes{};
    for (usize i = 0; i < sizeof(T); ++i) {
      bytes[i] = storage_[i];
    }
    return std::bit_cast<T>(bytes);
  }

 private:
  static constexpr usize kMaxAlign = std::max({alignof(Ts)...});
  static constexpr usize kMaxSize = std::max({sizeof(Ts)...});

  template <typename T>
  using Bytes = std::array<std::byte, sizeof(T)>;

  alignas(kMaxAlign) std::byte storage_[kMaxSize];
};

}  // namespace base
