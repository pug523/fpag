// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <algorithm>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

#include "fpag/base/debug/check.h"
#include "fpag/base/numeric.h"

namespace base {

namespace internal {

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
  static void destroy(usize, void*) noexcept {
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

// Move constructor helper.
template <usize Index, typename... Ts>
struct UnionMove;

template <usize Index>
struct UnionMove<Index> {
  static void move_construct(usize, void*, void*) noexcept {
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

}  // namespace internal

template <typename... Ts>
class TaggedUnion {
 public:
  static_assert(sizeof...(Ts) > 0, "TaggedUnion requires at least one type.");

  enum class Tag : usize {};

  template <typename T>
  static constexpr Tag TagOf =
      static_cast<Tag>(internal::TypeIndex<std::decay_t<T>, Ts...>::value);

  // Type-safe construction for any contained type.
  template <
      typename T,
      typename = std::enable_if_t<internal::ContainsType<T, Ts...>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  TaggedUnion(T&& value) noexcept
      : tag_(internal::TypeIndex<std::decay_t<T>, Ts...>::value) {
    using CleanT = std::decay_t<T>;
    ::new (static_cast<void*>(storage_)) CleanT(std::forward<T>(value));
  }

  TaggedUnion(const TaggedUnion&) = delete;
  TaggedUnion& operator=(const TaggedUnion&) = delete;

  TaggedUnion(TaggedUnion&& other) noexcept : tag_(other.tag_) {
    internal::UnionMove<0, Ts...>::move_construct(
        other.tag_, static_cast<void*>(other.storage_),
        static_cast<void*>(storage_));
  }

  TaggedUnion& operator=(TaggedUnion&& other) noexcept {
    if (this != &other) {
      destroy_current();
      tag_ = other.tag_;
      internal::UnionMove<0, Ts...>::move_construct(
          other.tag_, static_cast<void*>(other.storage_),
          static_cast<void*>(storage_));
    }
    return *this;
  }

  ~TaggedUnion() noexcept { destroy_current(); }

  // Returns current active tag.
  Tag tag() const noexcept { return static_cast<Tag>(tag_); }

  // Returns current active raw tag index.
  usize tag_raw() const noexcept { return tag_; }

  // Check if current active type is T.
  template <typename T>
  bool is() const noexcept {
    static_assert(internal::ContainsType<T, Ts...>::value,
                  "Type T is not part of TaggedUnion.");
    return tag_ == internal::TypeIndex<T, Ts...>::value;
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
  T get() && noexcept {
    FPAG_DCHECK(is<T>());
    return std::move(*reinterpret_cast<T*>(storage_));
  }

 private:
  void destroy_current() noexcept {
    internal::UnionDestructor<0, Ts...>::destroy(tag_,
                                                 static_cast<void*>(storage_));
  }

  static constexpr usize kStorageSize = std::max({sizeof(Ts)...});
  static constexpr usize kStorageAlign = std::max({alignof(Ts)...});

  alignas(kStorageAlign) u8 storage_[kStorageSize];
  usize tag_;
};

}  // namespace base
