// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>  // IWYU pragma: keep
#include <type_traits>
#include <utility>

#include "fpag/base/attributes.h"
#include "fpag/base/numeric.h"
#include "fpag/base/tagged_union_internal.h"
#include "fpag/debug/check.h"

namespace base {

namespace internal {

// Safe sizeof helper
template <typename T>
struct SafeSizeOfImpl {
  static constexpr usize value = sizeof(T);
};

template <>
struct SafeSizeOfImpl<void> {
  static constexpr usize value = 0;
};

template <typename T>
inline constexpr usize SafeSizeOf = SafeSizeOfImpl<T>::value;

// Safe alignof helper
template <typename T>
struct SafeAlignOfImpl {
  static constexpr usize value = alignof(T);
};

template <>
struct SafeAlignOfImpl<void> {
  static constexpr usize value = 1;
};

template <typename T>
inline constexpr usize SafeAlignOf = SafeAlignOfImpl<T>::value;

// Safe void-handling type traits
template <typename T>
struct SafeIsCopyConstructible : std::is_copy_constructible<T> {};
template <>
struct SafeIsCopyConstructible<void> : std::true_type {};
template <typename T>
inline constexpr bool SafeIsCopyConstructible_v =
    SafeIsCopyConstructible<T>::value;

template <typename T>
struct SafeIsNothrowCopyConstructible : std::is_nothrow_copy_constructible<T> {
};
template <>
struct SafeIsNothrowCopyConstructible<void> : std::true_type {};
template <typename T>
inline constexpr bool SafeIsNothrowCopyConstructible_v =
    SafeIsNothrowCopyConstructible<T>::value;

template <typename T>
struct SafeIsMoveConstructible : std::is_move_constructible<T> {};
template <>
struct SafeIsMoveConstructible<void> : std::true_type {};
template <typename T>
inline constexpr bool SafeIsMoveConstructible_v =
    SafeIsMoveConstructible<T>::value;

template <typename T>
struct SafeIsNothrowMoveConstructible : std::is_nothrow_move_constructible<T> {
};
template <>
struct SafeIsNothrowMoveConstructible<void> : std::true_type {};
template <typename T>
inline constexpr bool SafeIsNothrowMoveConstructible_v =
    SafeIsNothrowMoveConstructible<T>::value;

template <typename T>
struct SafeIsCopyAssignable : std::is_copy_assignable<T> {};
template <>
struct SafeIsCopyAssignable<void> : std::true_type {};
template <typename T>
inline constexpr bool SafeIsCopyAssignable_v = SafeIsCopyAssignable<T>::value;

template <typename T>
struct SafeIsMoveAssignable : std::is_move_assignable<T> {};
template <>
struct SafeIsMoveAssignable<void> : std::true_type {};
template <typename T>
inline constexpr bool SafeIsMoveAssignable_v = SafeIsMoveAssignable<T>::value;

template <typename T>
struct SafeIsNothrowMoveAssignable : std::is_nothrow_move_assignable<T> {};
template <>
struct SafeIsNothrowMoveAssignable<void> : std::true_type {};
template <typename T>
inline constexpr bool SafeIsNothrowMoveAssignable_v =
    SafeIsNothrowMoveAssignable<T>::value;

template <typename T>
struct SafeIsNothrowDestructible : std::is_nothrow_destructible<T> {};
template <>
struct SafeIsNothrowDestructible<void> : std::true_type {};
template <typename T>
inline constexpr bool SafeIsNothrowDestructible_v =
    SafeIsNothrowDestructible<T>::value;

// Helper to access the type at index I in a parameter pack Ts...
template <usize I, typename... Ts>
struct TypeAt;

template <usize I, typename Head, typename... Tail>
struct TypeAt<I, Head, Tail...> : TypeAt<I - 1, Tail...> {};

template <typename Head, typename... Tail>
struct TypeAt<0, Head, Tail...> {
  using type = Head;
};

template <usize I, typename... Ts>
using TypeAt_t = typename TypeAt<I, Ts...>::type;

// Zero-sized storage optimization wrapper
struct EmptyStorage {};

template <usize Align, usize Size>
struct UnionStorage {
  alignas(Align) std::byte data[Size];

  constexpr void* raw() noexcept { return data; }
  constexpr const void* raw() const noexcept { return data; }
};

template <usize Align>
struct UnionStorage<Align, 0> {
  FPAG_EMPTY_MEMBER EmptyStorage dummy;

  constexpr void* raw() noexcept { return nullptr; }
  constexpr const void* raw() const noexcept { return nullptr; }
};

}  // namespace internal

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

  // Construct from void (tag-only initialization)
  template <
      typename T = void,
      typename = std::enable_if_t<std::is_void_v<T> &&
                                  internal::ContainsType<void, Ts...>::value>>
  constexpr TaggedUnion() noexcept
      : storage_{},
        tag_(static_cast<TagStorageType>(
            internal::TypeIndex<void, Ts...>::value)) {}

  // Type-safe value construction for any contained type except void
  template <
      typename T,
      typename = std::enable_if_t<!std::is_void_v<std::decay_t<T>> &&
                                  internal::ContainsType<T, Ts...>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  constexpr TaggedUnion(T&& value) noexcept(
      std::is_nothrow_constructible_v<std::decay_t<T>, T>)
      : storage_{},
        tag_(static_cast<TagStorageType>(
            internal::TypeIndex<std::decay_t<T>, Ts...>::value)) {
    using CleanT = std::decay_t<T>;
    if constexpr (!std::is_empty_v<CleanT>) {
      std::construct_at(reinterpret_cast<CleanT*>(storage_.raw()),
                        std::forward<T>(value));
    }
  }

  // Copy Constructor
  constexpr TaggedUnion(const TaggedUnion& other) noexcept(
      (internal::SafeIsNothrowCopyConstructible_v<Ts> && ...))
    requires((internal::SafeIsCopyConstructible_v<Ts> && ...))
      : storage_{}, tag_(other.tag_) {
    copy_construct_from(other);
  }

  // Deleted Copy Constructor when non-copyable
  constexpr TaggedUnion(const TaggedUnion& other)
    requires(!(internal::SafeIsCopyConstructible_v<Ts> && ...))
  = delete;

  // Copy Assignment Operator
  constexpr TaggedUnion& operator=(const TaggedUnion& other) noexcept(
      (internal::SafeIsNothrowCopyConstructible_v<Ts> && ...) &&
      (internal::SafeIsNothrowDestructible_v<Ts> && ...))
    requires((internal::SafeIsCopyConstructible_v<Ts> && ...))
  {
    if (this != &other) {
      destroy_current();
      tag_ = other.tag_;
      copy_construct_from(other);
    }
    return *this;
  }

  // Deleted Copy Assignment when non-copyable
  constexpr TaggedUnion& operator=(const TaggedUnion& other)
    requires(!(internal::SafeIsCopyConstructible_v<Ts> && ...))
  = delete;

  // Move Constructor
  constexpr TaggedUnion(TaggedUnion&& other) noexcept(
      (internal::SafeIsNothrowMoveConstructible_v<Ts> && ...))
    requires((internal::SafeIsMoveConstructible_v<Ts> && ...))
      : storage_{}, tag_(other.tag_) {
    move_construct_from(std::move(other));
  }

  // Deleted Move Constructor when non-movable
  constexpr TaggedUnion(TaggedUnion&& other)
    requires(!(internal::SafeIsMoveConstructible_v<Ts> && ...))
  = delete;

  // Move Assignment Operator
  constexpr TaggedUnion& operator=(TaggedUnion&& other) noexcept(
      (internal::SafeIsNothrowMoveConstructible_v<Ts> && ...) &&
      (internal::SafeIsNothrowDestructible_v<Ts> && ...))
    requires((internal::SafeIsMoveConstructible_v<Ts> && ...))
  {
    if (this != &other) {
      destroy_current();
      tag_ = other.tag_;
      move_construct_from(std::move(other));
    }
    return *this;
  }

  // Deleted Move Assignment when non-movable
  constexpr TaggedUnion& operator=(TaggedUnion&& other)
    requires(!(internal::SafeIsMoveConstructible_v<Ts> && ...))
  = delete;

  constexpr ~TaggedUnion() noexcept { destroy_current(); }

  // Returns current active tag.
  constexpr Tag tag() const noexcept { return static_cast<Tag>(tag_); }

  // Returns current active raw tag index as usize.
  constexpr usize tag_raw() const noexcept { return static_cast<usize>(tag_); }

  // Checks if current active type is T.
  template <typename T>
  constexpr bool is() const noexcept {
    static_assert(internal::ContainsType<T, Ts...>::value,
                  "Type T is not part of TaggedUnion.");
    return static_cast<usize>(tag_) == internal::TypeIndex<T, Ts...>::value;
  }

  // Accessors for non-void contained types
  template <typename T>
    requires(!std::is_void_v<T>)
  constexpr T& get() & noexcept {
    FPAG_DCHECK(is<T>());
    return *std::launder(reinterpret_cast<T*>(storage_.raw()));
  }

  template <typename T>
    requires(!std::is_void_v<T>)
  constexpr const T& get() const& noexcept {
    FPAG_DCHECK(is<T>());
    return *std::launder(reinterpret_cast<const T*>(storage_.raw()));
  }

  template <typename T>
    requires(!std::is_void_v<T>)
  constexpr T&& get() && noexcept {
    FPAG_DCHECK(is<T>());
    return std::move(*std::launder(reinterpret_cast<T*>(storage_.raw())));
  }

  // Accessors for void
  template <typename T>
    requires(std::is_void_v<T>)
  constexpr void get() const noexcept {
    FPAG_DCHECK(is<void>());
  }

 private:
  constexpr void destroy_current() noexcept {
    auto destroy_helper = [this]<usize... Is>(std::index_sequence<Is...>) {
      usize current_tag = static_cast<usize>(tag_);
      [[maybe_unused]] const bool _ =
          ((Is == current_tag ? (destroy_at_index<Is>(), true) : false) || ...);
    };
    destroy_helper(std::index_sequence_for<Ts...>{});
  }

  template <usize I>
  constexpr void destroy_at_index() noexcept {
    using T = internal::TypeAt_t<I, Ts...>;
    if constexpr (!std::is_void_v<T> && !std::is_trivially_destructible_v<T>) {
      std::destroy_at(std::launder(reinterpret_cast<T*>(storage_.raw())));
    }
  }

  constexpr void move_construct_from(TaggedUnion&& other) noexcept(
      (internal::SafeIsNothrowMoveConstructible_v<Ts> && ...)) {
    auto move_helper = [this, &other]<usize... Is>(std::index_sequence<Is...>) {
      usize current_tag = static_cast<usize>(tag_);
      [[maybe_unused]] const bool _ =
          ((Is == current_tag ? (move_at_index<Is>(std::move(other)), true)
                              : false) ||
           ...);
    };
    move_helper(std::index_sequence_for<Ts...>{});
  }

  template <usize I>
  constexpr void move_at_index(TaggedUnion&& other) noexcept {
    using T = internal::TypeAt_t<I, Ts...>;
    if constexpr (!std::is_void_v<T> && !std::is_empty_v<T>) {
      std::construct_at(
          reinterpret_cast<T*>(storage_.raw()),
          std::move(*std::launder(reinterpret_cast<T*>(other.storage_.raw()))));
    }
  }

  constexpr void copy_construct_from(const TaggedUnion& other) noexcept(
      (internal::SafeIsNothrowCopyConstructible_v<Ts> && ...)) {
    auto copy_helper = [this, &other]<usize... Is>(std::index_sequence<Is...>) {
      usize current_tag = static_cast<usize>(tag_);
      [[maybe_unused]] const bool _ =
          ((Is == current_tag ? (copy_at_index<Is>(other), true) : false) ||
           ...);
    };
    copy_helper(std::index_sequence_for<Ts...>{});
  }

  template <usize I>
  constexpr void copy_at_index(const TaggedUnion& other) noexcept {
    using T = internal::TypeAt_t<I, Ts...>;
    if constexpr (!std::is_void_v<T> && !std::is_empty_v<T>) {
      std::construct_at(
          reinterpret_cast<T*>(storage_.raw()),
          *std::launder(reinterpret_cast<const T*>(other.storage_.raw())));
    }
  }

  static constexpr usize kStorageMaxAlign =
      std::max({internal::SafeAlignOf<Ts>...});
  static constexpr usize kStorageMaxSize =
      std::max({internal::SafeSizeOf<Ts>...});

  FPAG_EMPTY_MEMBER internal::UnionStorage<kStorageMaxAlign, kStorageMaxSize>
      storage_;
  TagStorageType tag_;
};

// Default AutoTaggedUnion providing automatically generated enum tag.
template <typename Head, typename... Tail>
using AutoTaggedUnion =
    TaggedUnion<typename internal::DefaultTagEnum<Head, Tail...>::Type,
                Head,
                Tail...>;

}  // namespace base
