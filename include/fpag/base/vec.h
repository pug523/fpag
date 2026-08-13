// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "fpag/base/idx.h"
#include "fpag/base/idx_range.h"
#include "fpag/base/numeric.h"
#include "fpag/base/vec_slice.h"
#include "fpag/debug/check.h"

namespace base {

template <typename T, HasIdxType Idx, typename Allocator = std::allocator<T>>
class Vec {
 public:
  using value_type = T;
  using allocator_type = Allocator;
  using size_type = usize;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = typename std::vector<T, Allocator>::iterator;
  using const_iterator = typename std::vector<T, Allocator>::const_iterator;
  using reverse_iterator = typename std::vector<T, Allocator>::reverse_iterator;
  using const_reverse_iterator =
      typename std::vector<T, Allocator>::const_reverse_iterator;
  using IdxType = typename Idx::IdxType;

  Vec() = default;
  ~Vec() = default;

  Vec(std::initializer_list<T> init, const Allocator& alloc = Allocator())
      : vec_(init, alloc) {}

  Vec(const Vec&) = delete;
  Vec& operator=(const Vec&) = delete;
  Vec(Vec&&) noexcept = default;
  Vec& operator=(Vec&&) noexcept = default;

  [[nodiscard]] const T& operator[](const Idx idx) const {
    FPAG_DCHECK_LT(static_cast<usize>(idx.idx), vec_.size());
    return vec_[idx.idx];
  }

  [[nodiscard]] T& operator[](const Idx idx) {
    FPAG_DCHECK_LT(static_cast<usize>(idx.idx), vec_.size());
    return vec_[idx.idx];
  }

  [[nodiscard]] const T& operator[](usize offset) const {
    FPAG_DCHECK_LT(offset, vec_.size());
    return vec_[offset];
  }

  [[nodiscard]] T& operator[](usize offset) {
    FPAG_DCHECK_LT(offset, vec_.size());
    return vec_[offset];
  }

  constexpr void reserve(usize size) { vec_.reserve(size); }
  constexpr void resize(usize size) { vec_.resize(size); }
  constexpr void resize(usize size, const T& init_value) {
    vec_.resize(size, init_value);
  }

  template <typename... Args>
  constexpr Idx emplace_back(Args&&... args) {
    vec_.emplace_back(std::forward<Args>(args)...);
    return Idx(static_cast<IdxType>(size() - 1));
  }

  constexpr Idx push_back(const T& value) { return emplace_back(value); }
  constexpr Idx push_back(T&& value) { return emplace_back(std::move(value)); }

  constexpr void pop_back() {
    FPAG_DCHECK(!empty());
    vec_.pop_back();
  }

  constexpr void clear() noexcept { vec_.clear(); }
  constexpr void shrink_to_fit() { vec_.shrink_to_fit(); }

  [[nodiscard]] constexpr usize size() const noexcept { return vec_.size(); }
  [[nodiscard]] constexpr usize capacity() const noexcept {
    return vec_.capacity();
  }
  [[nodiscard]] constexpr bool empty() const noexcept { return vec_.empty(); }

  [[nodiscard]] constexpr reference front() {
    FPAG_DCHECK(!empty());
    return vec_.front();
  }
  [[nodiscard]] constexpr const_reference front() const {
    FPAG_DCHECK(!empty());
    return vec_.front();
  }

  [[nodiscard]] constexpr reference back() {
    FPAG_DCHECK(!empty());
    return vec_.back();
  }
  [[nodiscard]] constexpr const_reference back() const {
    FPAG_DCHECK(!empty());
    return vec_.back();
  }

  constexpr iterator begin() noexcept { return vec_.begin(); }
  constexpr iterator end() noexcept { return vec_.end(); }
  constexpr const_iterator begin() const noexcept { return vec_.begin(); }
  constexpr const_iterator end() const noexcept { return vec_.end(); }
  constexpr const_iterator cbegin() const noexcept { return vec_.cbegin(); }
  constexpr const_iterator cend() const noexcept { return vec_.cend(); }

  constexpr reverse_iterator rbegin() noexcept { return vec_.rbegin(); }
  constexpr reverse_iterator rend() noexcept { return vec_.rend(); }
  constexpr const_reverse_iterator rbegin() const noexcept {
    return vec_.rbegin();
  }
  constexpr const_reverse_iterator rend() const noexcept { return vec_.rend(); }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return vec_.crbegin();
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return vec_.crend();
  }

  [[nodiscard]] constexpr IdxRange<Idx> idx_range() const noexcept {
    return IdxRange<Idx>(Idx{0}, static_cast<IdxType>(vec_.size()));
  }

  [[nodiscard]] constexpr const T* data() const noexcept { return vec_.data(); }
  [[nodiscard]] constexpr T* data() noexcept { return vec_.data(); }

  [[nodiscard]] constexpr VecSlice<T, Idx> slice(IdxType offset, IdxType size) {
    FPAG_DCHECK_LE(static_cast<usize>(offset + size), vec_.size());
    return VecSlice<T, Idx>(data() + offset, size);
  }
  [[nodiscard]] constexpr VecSlice<T, Idx> slice() {
    return slice(0, static_cast<IdxType>(size()));
  }

  [[nodiscard]] constexpr ConstVecSlice<T, Idx> slice(IdxType offset,
                                                      IdxType size) const {
    FPAG_DCHECK_LE(static_cast<usize>(offset + size), vec_.size());
    return ConstVecSlice<T, Idx>(data() + offset, size);
  }
  [[nodiscard]] constexpr ConstVecSlice<T, Idx> slice() const {
    return slice(0, static_cast<IdxType>(size()));
  }

 private:
  std::vector<T, Allocator> vec_;
};

}  // namespace base
