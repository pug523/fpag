// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "fpag/base/idx.h"
#include "fpag/base/idx_range.h"
#include "fpag/base/numeric.h"
#include "fpag/base/vec_slice.h"

namespace base {

template <typename T, HasIdxType Idx, typename Allocator = std::allocator<T>>
class Vec {
 public:
  using IdxType = typename Idx::IdxType;

  Vec() = default;
  ~Vec() = default;
  Vec(const Vec&) = delete;
  Vec& operator=(const Vec&) = delete;
  Vec(Vec&&) noexcept = default;
  Vec& operator=(Vec&&) noexcept = default;

  const T& operator[](const Idx idx) const { return vec_[idx.idx]; }
  T& operator[](const Idx idx) { return vec_[idx.idx]; }

  constexpr void reserve(usize size) { vec_.reserve(size); }
  constexpr void resize(usize size) { vec_.resize(size); }
  constexpr void resize(usize size, T init_value) {
    vec_.resize(size, init_value);
  }

  template <typename... Args>
  constexpr Idx emplace_back(Args&&... args) {
    vec_.emplace_back(std::forward<Args>(args)...);
    return Idx(static_cast<IdxType>(size() - 1));
  }

  constexpr void pop_back() { vec_.pop_back(); }
  constexpr void shrink_to_fit() { vec_.shrink_to_fit(); }

  constexpr usize size() const { return vec_.size(); }
  constexpr usize capacity() const { return vec_.capacity(); }

  constexpr auto begin() const { return vec_.begin(); }
  constexpr auto end() const { return vec_.end(); }

  constexpr IdxRange<Idx> idx_range() const {
    return IdxRange<Idx>(Idx{0}, static_cast<IdxType>(vec_.size()));
  }

  constexpr const T* data() const { return vec_.data(); }
  constexpr T* data() { return vec_.data(); }

  constexpr VecSlice<T, Idx> slice(IdxType offset, IdxType size) {
    return VecSlice<T, Idx>(data() + offset, size);
  }
  constexpr VecSlice<T, Idx> slice() { return slice(0, size()); }

  constexpr ConstVecSlice<T, Idx> slice(IdxType offset, IdxType size) const {
    return ConstVecSlice<T, Idx>(data() + offset, size);
  }
  constexpr ConstVecSlice<T, Idx> slice() const { return slice(0, size()); }

 private:
  std::vector<T, Allocator> vec_;
};

}  // namespace base
