// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "fpag/base/debug/check.h"
#include "fpag/base/numeric.h"
#include "fpag/base/tagged_union_internal.h"

namespace base {

// Generic TaggedUnion implementation supporting explicit or auto-generated
// TagEnum.
template <typename TagEnum, typename... Ts>
class TaggedUnion {
 public:
  static_assert(sizeof...(Ts) > 0, "TaggedUnion requires at least one type.");
  static_assert(internal::validate_tag_enum<TagEnum, sizeof...(Ts)>(),
                "TagEnum validation failed.");

  using Tag = TagEnum;
  using TagStorageType = internal::TagUnderlyingType_t<TagEnum>;

  template <typename T>
  static constexpr Tag TagOf =
      static_cast<Tag>(internal::TypeIndex<std::decay_t<T>, Ts...>::value);

  // Type-safe construction for any contained type.
  template <
      typename T,
      typename = std::enable_if_t<internal::ContainsType<T, Ts...>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  TaggedUnion(T&& value) noexcept
      : tag_(static_cast<TagStorageType>(
            internal::TypeIndex<std::decay_t<T>, Ts...>::value)) {
    using CleanT = std::decay_t<T>;
    ::new (static_cast<void*>(storage_)) CleanT(std::forward<T>(value));
  }

  TaggedUnion(const TaggedUnion&) = delete;
  TaggedUnion& operator=(const TaggedUnion&) = delete;

  TaggedUnion(TaggedUnion&& other) noexcept : tag_(other.tag_) {
    internal::UnionMove<0, Ts...>::move_construct(
        static_cast<usize>(other.tag_), static_cast<void*>(other.storage_),
        static_cast<void*>(storage_));
  }

  TaggedUnion& operator=(TaggedUnion&& other) noexcept {
    if (this != &other) {
      destroy_current();
      tag_ = other.tag_;
      internal::UnionMove<0, Ts...>::move_construct(
          static_cast<usize>(other.tag_), static_cast<void*>(other.storage_),
          static_cast<void*>(storage_));
    }
    return *this;
  }

  ~TaggedUnion() noexcept { destroy_current(); }

  // Returns current active tag.
  Tag tag() const noexcept { return static_cast<Tag>(tag_); }

  // Returns current active raw tag index as usize.
  usize tag_raw() const noexcept { return static_cast<usize>(tag_); }

  // Check if current active type is T.
  template <typename T>
  bool is() const noexcept {
    static_assert(internal::ContainsType<T, Ts...>::value,
                  "Type T is not part of TaggedUnion.");
    return static_cast<usize>(tag_) == internal::TypeIndex<T, Ts...>::value;
  }

  // Get reference to contained type T.
  template <typename T>
  T& get() & noexcept {
    FPAG_DCHECK(is<T>());
    return *reinterpret_cast<T*>(storage_);
  }

  template <typename T>
  const T& get() const& noexcept {
    FPAG_DCHECK(is<T>());
    return *reinterpret_cast<const T*>(storage_);
  }

  template <typename T>
  T&& get() && noexcept {
    FPAG_DCHECK(is<T>());
    return std::move(*reinterpret_cast<T*>(storage_));
  }

 private:
  void destroy_current() noexcept {
    internal::UnionDestructor<0, Ts...>::destroy(static_cast<usize>(tag_),
                                                 static_cast<void*>(storage_));
  }

  static constexpr usize kStorageSize = std::max({sizeof(Ts)...});
  static constexpr usize kStorageAlign = std::max({alignof(Ts)...});

  alignas(kStorageAlign) u8 storage_[kStorageSize];
  TagStorageType tag_;
};

// Default AutoTaggedUnion alias providing automatically generated enum tag.
template <typename Head, typename... Tail>
class AutoTaggedUnion
    : public TaggedUnion<typename internal::DefaultTagEnum<Head, Tail...>::Type,
                         Head,
                         Tail...> {
  using Base =
      TaggedUnion<typename internal::DefaultTagEnum<Head, Tail...>::Type,
                  Head,
                  Tail...>;

 public:
  using Base::Base;
};

}  // namespace base
