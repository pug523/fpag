// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <type_traits>
#include <utility>

#include "fpag/base/debug/check.h"
#include "fpag/base/tagged_union.h"

namespace base {
//
// Lightweight wrapper types to assist construction
template <typename T>
struct Ok {
  T value;
};

template <typename E>
struct Err {
  E error;
};

// Type deduction helpers
template <typename T>
Ok<std::decay_t<T>> make_ok(T&& value) noexcept {
  return Ok<std::decay_t<T>>{std::forward<T>(value)};
}

template <typename E>
Err<std::decay_t<E>> make_err(E&& error) noexcept {
  return Err<std::decay_t<E>>{std::forward<E>(error)};
}

template <typename T, typename E>
class Result {
 public:
  using StorageUnion = TaggedUnion<Ok<T>, Err<E>>;

  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  Result(Ok<T>&& ok) noexcept : union_(std::move(ok)) {}

  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  Result(const Ok<T>& ok) noexcept : union_(Ok<T>{ok.value}) {}

  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  Result(Err<E>&& err) noexcept : union_(std::move(err)) {}

  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  Result(const Err<E>& err) noexcept : union_(Err<E>{err.error}) {}

  Result(const Result&) = delete;
  Result& operator=(const Result&) = delete;

  Result(Result&& other) noexcept = default;
  Result& operator=(Result&& other) noexcept = default;

  ~Result() noexcept = default;

  // Status checks
  bool is_ok() const noexcept { return union_.template is<Ok<T>>(); }
  bool is_err() const noexcept { return union_.template is<Err<E>>(); }

  // Unwraps
  T unwrap() && noexcept {
    FPAG_DCHECK(is_ok());
    return std::move(union_).template get<Ok<T>>().value;
  }

  // Unwraps error
  E unwrap_err() && noexcept {
    FPAG_DCHECK(is_err());
    return std::move(union_).template get<Err<E>>().error;
  }

  // Unwraps with fallback default value
  T unwrap_or(T default_val) && noexcept {
    if (is_ok()) {
      return std::move(*this).unwrap();
    }
    return std::move(default_val);
  }

  // Non-destructive inspection
  const T& value() const& noexcept {
    FPAG_DCHECK(is_ok());
    return union_.template get<Ok<T>>().value;
  }

  T& value() & noexcept {
    FPAG_DCHECK(is_ok());
    return union_.template get<Ok<T>>().value;
  }

  // Maps T into U if Ok, keep Err if Err
  template <typename F>
  auto map(F&& f) const& noexcept
      -> Result<std::decay_t<decltype(f(std::declval<const T&>()))>, E> {
    // using U = std::decay_t<decltype(f(std::declval<const T&>()))>;
    if (is_ok()) {
      return make_ok(f(value()));
    }
    return make_err(union_.template get<Err<E>>().error);
  }

  template <typename F>
  auto map(F&& f) && noexcept
      -> Result<std::decay_t<decltype(f(std::declval<T>()))>, E> {
    // using U = std::decay_t<decltype(f(std::declval<T>()))>;
    if (is_ok()) {
      return make_ok(f(std::move(*this).unwrap()));
    }
    return make_err(std::move(*this).unwrap_err());
  }

  // Chain another Result-returning operation
  template <typename F>
  auto and_then(F&& f) const& noexcept
      -> decltype(f(std::declval<const T&>())) {
    if (is_ok()) {
      return f(value());
    }
    return make_err(union_.template get<Err<E>>().error);
  }

  template <typename F>
  auto and_then(F&& f) && noexcept -> decltype(f(std::declval<T>())) {
    if (is_ok()) {
      return f(std::move(*this).unwrap());
    }
    return make_err(std::move(*this).unwrap_err());
  }

 private:
  StorageUnion union_;
};

}  // namespace base
