// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <utility>

#include "fpag/base/idx.h"
#include "fpag/base/idx_range.h"
#include "fpag/base/numeric.h"
#include "fpag/base/soo_vec_slice.h"
#include "fpag/debug/check.h"

namespace base {

template <typename T,
          HasIdxType Idx,
          usize N = 8,
          typename Allocator = std::allocator<T>>
class SooVec {
 public:
  using value_type = T;
  using allocator_type = Allocator;
  using size_type = usize;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using IdxType = typename Idx::IdxType;

  constexpr SooVec() noexcept = default;

  constexpr SooVec(std::initializer_list<T> init) {
    reserve(init.size());
    for (const auto& item : init) {
      emplace_back(item);
    }
  }

  constexpr ~SooVec() { clear(); }

  constexpr SooVec(const SooVec& other) {
    reserve(other.size_);
    for (usize i = 0; i < other.size_; ++i) {
      emplace_back(other[Idx(static_cast<IdxType>(i))]);
    }
  }

  constexpr SooVec& operator=(const SooVec& other) {
    if (this != &other) {
      clear();
      reserve(other.size_);
      for (usize i = 0; i < other.size_; ++i) {
        emplace_back(other[Idx(static_cast<IdxType>(i))]);
      }
    }
    return *this;
  }

  constexpr SooVec(SooVec&& other) noexcept { move_from(std::move(other)); }

  constexpr SooVec& operator=(SooVec&& other) noexcept {
    if (this != &other) {
      clear();
      move_from(std::move(other));
    }
    return *this;
  }

  [[nodiscard]] constexpr const T& operator[](const Idx idx) const {
    FPAG_DCHECK_LT(static_cast<usize>(idx.idx), size_);
    return data()[idx.idx];
  }

  [[nodiscard]] constexpr T& operator[](const Idx idx) {
    FPAG_DCHECK_LT(static_cast<usize>(idx.idx), size_);
    return data()[idx.idx];
  }

  [[nodiscard]] constexpr const T& operator[](usize offset) const {
    FPAG_DCHECK_LT(offset, size_);
    return data()[offset];
  }

  [[nodiscard]] constexpr T& operator[](usize offset) {
    FPAG_DCHECK_LT(offset, size_);
    return data()[offset];
  }

  constexpr void reserve(usize new_capacity) {
    if (new_capacity <= capacity_) {
      return;
    }

    T* new_dynamic = alloc_.allocate(new_capacity);
    T* current_data = data();

    for (usize i = 0; i < size_; ++i) {
      std::construct_at(new_dynamic + i, std::move(current_data[i]));
      std::destroy_at(current_data + i);
    }

    if (is_dynamic()) {
      alloc_.deallocate(dynamic_data_, capacity_);
    }

    dynamic_data_ = new_dynamic;
    capacity_ = new_capacity;
  }

  template <typename... Args>
  constexpr Idx emplace_back(Args&&... args) {
    if (size_ == capacity_) {
      reserve(capacity_ == 0 ? N : capacity_ * 2);
    }
    std::construct_at(data() + size_, std::forward<Args>(args)...);
    const usize new_idx = size_++;
    return Idx(static_cast<IdxType>(new_idx));
  }

  constexpr Idx push_back(const T& value) { return emplace_back(value); }
  constexpr Idx push_back(T&& value) { return emplace_back(std::move(value)); }

  constexpr void pop_back() {
    FPAG_DCHECK_GT(size_, 0u);
    --size_;
    std::destroy_at(data() + size_);
  }

  constexpr void clear() noexcept {
    T* current_data = data();
    for (usize i = 0; i < size_; ++i) {
      std::destroy_at(current_data + i);
    }
    if (is_dynamic()) {
      alloc_.deallocate(dynamic_data_, capacity_);
      dynamic_data_ = nullptr;
    }
    size_ = 0;
    capacity_ = N;
  }

  [[nodiscard]] constexpr usize size() const noexcept { return size_; }
  [[nodiscard]] constexpr usize capacity() const noexcept { return capacity_; }
  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

  [[nodiscard]] constexpr reference front() noexcept {
    FPAG_DCHECK(!empty());
    return data()[0];
  }

  [[nodiscard]] constexpr const_reference front() const noexcept {
    FPAG_DCHECK(!empty());
    return data()[0];
  }

  [[nodiscard]] constexpr reference back() noexcept {
    FPAG_DCHECK(!empty());
    return data()[size_ - 1];
  }

  [[nodiscard]] constexpr const_reference back() const noexcept {
    FPAG_DCHECK(!empty());
    return data()[size_ - 1];
  }

  [[nodiscard]] constexpr const T* data() const noexcept {
    return is_dynamic()
               ? dynamic_data_
               : std::launder(reinterpret_cast<const T*>(inline_storage_));
  }

  [[nodiscard]] constexpr T* data() noexcept {
    return is_dynamic() ? dynamic_data_
                        : std::launder(reinterpret_cast<T*>(inline_storage_));
  }

  constexpr iterator begin() noexcept { return data(); }
  constexpr iterator end() noexcept { return data() + size_; }
  constexpr const_iterator begin() const noexcept { return data(); }
  constexpr const_iterator end() const noexcept { return data() + size_; }
  constexpr const_iterator cbegin() const noexcept { return data(); }
  constexpr const_iterator cend() const noexcept { return data() + size_; }

  constexpr reverse_iterator rbegin() noexcept {
    return reverse_iterator(end());
  }
  constexpr reverse_iterator rend() noexcept {
    return reverse_iterator(begin());
  }
  constexpr const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  constexpr const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(cend());
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(cbegin());
  }

  [[nodiscard]] constexpr IdxRange<Idx> idx_range() const noexcept {
    return IdxRange<Idx>(Idx{0}, static_cast<IdxType>(size_));
  }

  constexpr SooVecSlice<T, Idx> slice(IdxType offset, IdxType size) {
    FPAG_DCHECK_LE(static_cast<usize>(offset + size), size_);
    return SooVecSlice<T, Idx>(data() + offset, size);
  }
  constexpr SooVecSlice<T, Idx> slice() {
    return slice(0, static_cast<IdxType>(size_));
  }

  constexpr ConstSooVecSlice<T, Idx> slice(IdxType offset, IdxType size) const {
    FPAG_DCHECK_LE(static_cast<usize>(offset + size), size_);
    return ConstSooVecSlice<T, Idx>(data() + offset, size);
  }
  constexpr ConstSooVecSlice<T, Idx> slice() const {
    return slice(0, static_cast<IdxType>(size_));
  }

 private:
  [[nodiscard]] constexpr bool is_dynamic() const noexcept {
    return capacity_ > N;
  }

  constexpr void move_from(SooVec&& other) noexcept {
    size_ = other.size_;
    capacity_ = other.capacity_;

    if (other.is_dynamic()) {
      dynamic_data_ = other.dynamic_data_;
      other.dynamic_data_ = nullptr;
    } else {
      T* this_inline = std::launder(reinterpret_cast<T*>(inline_storage_));
      T* other_inline =
          std::launder(reinterpret_cast<T*>(other.inline_storage_));
      for (usize i = 0; i < size_; ++i) {
        std::construct_at(this_inline + i, std::move(other_inline[i]));
        std::destroy_at(other_inline + i);
      }
    }
    other.size_ = 0;
    other.capacity_ = N;
  }

  [[no_unique_address]] Allocator alloc_{};
  usize size_ = 0;
  usize capacity_ = N;

  union {
    // NOLINTNEXTLINE(runtime/arrays)
    alignas(T) std::byte inline_storage_[sizeof(T) * (N > 0 ? N : 1)];
    T* dynamic_data_;
  };
};

}  // namespace base
