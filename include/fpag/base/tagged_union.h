// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
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

  // Type-safe value construction for any contained type.
  template <
      typename T,
      typename = std::enable_if_t<internal::ContainsType<T, Ts...>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  constexpr TaggedUnion(T&& value) noexcept(
      std::is_nothrow_constructible_v<std::decay_t<T>, T>)
      : tag_(static_cast<TagStorageType>(
            internal::TypeIndex<std::decay_t<T>, Ts...>::value)) {
    using CleanT = std::decay_t<T>;
    std::construct_at(reinterpret_cast<CleanT*>(storage_),
                      std::forward<T>(value));
  }

  constexpr TaggedUnion(const TaggedUnion& other) noexcept(
      (std::is_nothrow_copy_constructible_v<Ts> && ...))
    requires((std::is_copy_constructible_v<Ts> && ...))
      : tag_(other.tag_) {
    copy_construct_from(other);
  }

  constexpr TaggedUnion& operator=(const TaggedUnion& other) noexcept(
      (std::is_nothrow_copy_constructible_v<Ts> && ...) &&
      (std::is_nothrow_destructible_v<Ts> && ...))
    requires((std::is_copy_constructible_v<Ts> && ...) &&
             (std::is_copy_assignable_v<Ts> && ...))
  {
    if (this != &other) {
      destroy_current();
      tag_ = other.tag_;
      copy_construct_from(other);
    }
    return *this;
  }

  constexpr TaggedUnion(TaggedUnion&& other) noexcept(
      (std::is_nothrow_move_constructible_v<Ts> && ...))
      : tag_(other.tag_) {
    move_construct_from(std::move(other));
  }

  constexpr TaggedUnion& operator=(TaggedUnion&& other) noexcept(
      (std::is_nothrow_move_constructible_v<Ts> && ...) &&
      (std::is_nothrow_destructible_v<Ts> && ...)) {
    if (this != &other) {
      destroy_current();
      tag_ = other.tag_;
      move_construct_from(std::move(other));
    }
    return *this;
  }

  constexpr ~TaggedUnion() noexcept { destroy_current(); }

  // Returns current active tag.
  inline constexpr Tag tag() const noexcept { return static_cast<Tag>(tag_); }

  // Returns current active raw tag index as usize.
  inline constexpr usize tag_raw() const noexcept {
    return static_cast<usize>(tag_);
  }

  // Checks if current active type is T.
  template <typename T>
  inline constexpr bool is() const noexcept {
    static_assert(internal::ContainsType<T, Ts...>::value,
                  "Type T is not part of TaggedUnion.");
    return static_cast<usize>(tag_) == internal::TypeIndex<T, Ts...>::value;
  }

  // Accessors for contained type T with pointer laundering.
  template <typename T>
  inline constexpr T& get() & noexcept {
    FPAG_DCHECK(is<T>());
    return *std::launder(reinterpret_cast<T*>(storage_));
  }

  template <typename T>
  inline constexpr const T& get() const& noexcept {
    FPAG_DCHECK(is<T>());
    return *std::launder(reinterpret_cast<const T*>(storage_));
  }

  template <typename T>
  inline constexpr T&& get() && noexcept {
    FPAG_DCHECK(is<T>());
    return std::move(*std::launder(reinterpret_cast<T*>(storage_)));
  }

 private:
  constexpr void destroy_current() noexcept {
    auto destroy_helper = [this]<usize... Is>(std::index_sequence<Is...>) {
      usize current_tag = static_cast<usize>(tag_);
      bool _ = ((Is == current_tag ? (std::destroy_at(std::launder(
                                          reinterpret_cast<Ts*>(storage_))),
                                      true)
                                   : false) ||
                ...);
    };
    destroy_helper(std::index_sequence_for<Ts...>{});
  }

  constexpr void move_construct_from(TaggedUnion&& other) noexcept(
      (std::is_nothrow_move_constructible_v<Ts> && ...)) {
    auto move_helper = [this, &other]<usize... Is>(std::index_sequence<Is...>) {
      usize current_tag = static_cast<usize>(tag_);
      bool _ = ((Is == current_tag
                     ? (std::construct_at(
                            reinterpret_cast<Ts*>(storage_),
                            std::move(*std::launder(
                                reinterpret_cast<Ts*>(other.storage_)))),
                        true)
                     : false) ||
                ...);
    };
    move_helper(std::index_sequence_for<Ts...>{});
  }

  constexpr void copy_construct_from(const TaggedUnion& other) noexcept(
      (std::is_nothrow_copy_constructible_v<Ts> && ...)) {
    auto copy_helper = [this, &other]<usize... Is>(std::index_sequence<Is...>) {
      usize current_tag = static_cast<usize>(tag_);
      bool _ =
          ((Is == current_tag
                ? (std::construct_at(reinterpret_cast<Ts*>(storage_),
                                     *std::launder(reinterpret_cast<const Ts*>(
                                         other.storage_))),
                   true)
                : false) ||
           ...);
    };
    copy_helper(std::index_sequence_for<Ts...>{});
  }

  static constexpr usize kStorageSize = std::max({sizeof(Ts)...});

  alignas(Ts...) std::byte storage_[kStorageSize];
  TagStorageType tag_;
};

// Default AutoTaggedUnion providing automatically generated enum tag.
template <typename Head, typename... Tail>
using AutoTaggedUnion =
    TaggedUnion<typename internal::DefaultTagEnum<Head, Tail...>::Type,
                Head,
                Tail...>;

}  // namespace base
