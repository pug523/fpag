// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <type_traits>
#include <utility>

#include "fpag/base/debug/check.h"
#include "fpag/base/numeric.h"

namespace base {

enum class ResultTag : u8 {
  Ok,
  Err,
};

// Lightweight wrapper types to assist construction
template <typename T>
struct Ok {
  T value;
};

template <typename E>
struct Err {
  E error;
};

// Type deduction
template <typename T>
Ok<typename std::decay<T>::type> make_ok(T&& value) noexcept {
  return Ok<typename std::decay<T>::type>{std::forward<T>(value)};
}

template <typename E>
Err<typename std::decay<E>::type> make_err(E&& error) noexcept {
  return Err<typename std::decay<E>::type>{std::forward<E>(error)};
}

template <typename T, typename E>
class Result {
 public:
  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  Result(Ok<T>&& ok) noexcept : tag_(ResultTag::Ok) {
    ::new (static_cast<void*>(&storage_.ok_val_)) T(std::move(ok.value));
  }

  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  Result(const Ok<T>& ok) noexcept : tag_(ResultTag::Ok) {
    ::new (static_cast<void*>(&storage_.ok_val_)) T(ok.value);
  }

  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  Result(Err<E>&& err) noexcept : tag_(ResultTag::Err) {
    ::new (static_cast<void*>(&storage_.err_val_)) E(std::move(err.error));
  }

  // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
  Result(const Err<E>& err) noexcept : tag_(ResultTag::Err) {
    ::new (static_cast<void*>(&storage_.err_val_)) E(err.error);
  }

  Result(const Result&) = delete;
  Result& operator=(const Result&) = delete;

  Result(Result&& other) noexcept : tag_(other.tag_) {
    if (other.tag_ == ResultTag::Ok) {
      ::new (static_cast<void*>(&storage_.ok_val_))
          T(std::move(other.storage_.ok_val_));
    } else {
      ::new (static_cast<void*>(&storage_.err_val_))
          E(std::move(other.storage_.err_val_));
    }
  }

  // invokes destructors for union members manually.
  ~Result() noexcept { destroy(); }

  // Move assignment operator
  Result& operator=(Result&& other) noexcept {
    if (this != &other) {
      destroy();
      tag_ = other.tag_;
      if (other.tag_ == ResultTag::Ok) {
        ::new (static_cast<void*>(&storage_.ok_val_))
            T(std::move(other.storage_.ok_val_));
      } else {
        ::new (static_cast<void*>(&storage_.err_val_))
            E(std::move(other.storage_.err_val_));
      }
    }
    return *this;
  }

  // Status checks
  bool is_ok() const noexcept { return tag_ == ResultTag::Ok; }
  bool is_err() const noexcept { return tag_ == ResultTag::Err; }

  // Unwraps
  T unwrap() && noexcept {
    FPAG_DCHECK_EQ(tag_, ResultTag::Ok);
    return std::move(storage_.ok_val_);
  }

  // Unwraps error
  E unwrap_err() && noexcept {
    FPAG_DCHECK_EQ(tag_, ResultTag::Err);
    return std::move(storage_.err_val_);
  }

  // Unwraps with fallback default value
  T unwrap_or(T default_val) && noexcept {
    if (is_ok()) {
      return std::move(storage_.ok_val_);
    }
    return std::move(default_val);
  }

  // // Non-destructive inspection
  // const T& value() const& noexcept {
  //   FPAG_DCHECK_EQ(tag_, ResultTag::Ok);
  //   return storage_.ok_val_;
  // }

  // T& value() & noexcept {
  //   FPAG_DCHECK_EQ(tag_, ResultTag::Ok);
  //   return storage_.ok_val_;
  // }

  // Maps T into U if Ok, keep Err if Err
  template <typename F>
  auto map(F&& f) const noexcept -> Result<
      typename std::decay<decltype(f(std::declval<const T&>()))>::type,
      E> {
    if (is_ok()) {
      return make_ok(f(unwrap()));
    }
    return make_err(unwrap_err());
  }

  // Chain another Result-returning operation
  template <typename F>
  auto and_then(F&& f) const noexcept -> decltype(f(std::declval<const T&>())) {
    if (is_ok()) {
      return f(unwrap());
    }
    return make_err(unwrap_err());
  }

 private:
  void destroy() noexcept {
    if (tag_ == ResultTag::Ok) {
      storage_.ok_val_.~T();
    } else {
      storage_.err_val_.~E();
    }
  }

  // Tagged union storage
  union Storage {
    T ok_val_;
    E err_val_;

    Storage() noexcept {}
    ~Storage() noexcept {}
  };

  ResultTag tag_;
  Storage storage_;
};

}  // namespace base
