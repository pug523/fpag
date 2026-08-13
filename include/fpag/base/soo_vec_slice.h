// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <type_traits>

#include "fpag/base/idx.h"
#include "fpag/base/idx_range.h"
#include "fpag/base/numeric.h"
#include "fpag/debug/check.h"

namespace base {

template <typename T, HasIdxType Idx>
class SooVecSlice {
 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using IdxType = typename Idx::IdxType;

  constexpr SooVecSlice() noexcept = default;
  constexpr SooVecSlice(pointer data, usize size) noexcept
      : data_(data), size_(size) {}
  constexpr SooVecSlice(pointer begin, pointer end) noexcept
      : data_(begin),
        size_(static_cast<usize>(end >= begin ? end - begin : 0)) {
    FPAG_DCHECK_GE(end, begin);
  }

  template <typename Container>
    requires(!std::is_same_v<std::decay_t<Container>, SooVecSlice> &&
             requires(Container& c) {
               { c.data() } -> std::same_as<pointer>;
               { c.size() } -> std::convertible_to<usize>;
             })
  constexpr explicit SooVecSlice(Container& container) noexcept
      : data_(container.data()), size_(container.size()) {}

  constexpr ~SooVecSlice() = default;
  constexpr SooVecSlice(const SooVecSlice&) noexcept = default;
  constexpr SooVecSlice& operator=(const SooVecSlice&) noexcept = default;
  constexpr SooVecSlice(SooVecSlice&&) noexcept = default;
  constexpr SooVecSlice& operator=(SooVecSlice&&) noexcept = default;

  // Implicit conversion from SooVecSlice<T, Idx> to SooVecSlice<const T, Idx>
  template <typename U = T>
    requires std::is_const_v<U>
  constexpr SooVecSlice(  // NOLINT
      const SooVecSlice<std::remove_const_t<T>, Idx>& other) noexcept
      : data_(other.data()), size_(other.size()) {}

  [[nodiscard]] constexpr reference operator[](Idx idx) const noexcept {
    FPAG_DCHECK_LT(static_cast<usize>(idx.idx), size_);
    return data_[idx.idx];
  }

  [[nodiscard]] constexpr reference operator[](usize offset) const noexcept {
    FPAG_DCHECK_LT(offset, size_);
    return data_[offset];
  }

  [[nodiscard]] constexpr pointer data() const noexcept { return data_; }
  [[nodiscard]] constexpr usize size() const noexcept { return size_; }
  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

  [[nodiscard]] constexpr iterator begin() const noexcept { return data_; }
  [[nodiscard]] constexpr iterator end() const noexcept {
    return data_ + size_;
  }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
    return data_;
  }
  [[nodiscard]] constexpr const_iterator cend() const noexcept {
    return data_ + size_;
  }

  [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }
  [[nodiscard]] constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }
  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(cend());
  }
  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(cbegin());
  }

  [[nodiscard]] constexpr reference front() const noexcept {
    FPAG_DCHECK(!empty());
    return data_[0];
  }

  [[nodiscard]] constexpr reference back() const noexcept {
    FPAG_DCHECK(!empty());
    return data_[size_ - 1];
  }

  constexpr void pop_front(usize count = 1) noexcept {
    FPAG_DCHECK_LE(count, size_);
    data_ += count;
    size_ -= count;
  }

  constexpr void pop_back(usize count = 1) noexcept {
    FPAG_DCHECK_LE(count, size_);
    size_ -= count;
  }

  [[nodiscard]] constexpr IdxRange<Idx> idx_range() const noexcept {
    return IdxRange<Idx>(Idx{0}, static_cast<IdxType>(size_));
  }

  [[nodiscard]] constexpr SooVecSlice subslice(usize offset,
                                               usize count) const noexcept {
    FPAG_DCHECK_LE(offset, size_);
    FPAG_DCHECK_LE(count, size_ - offset);
    return SooVecSlice(data_ + offset, count);
  }

  [[nodiscard]] constexpr SooVecSlice subslice(
      IdxRange<Idx> range) const noexcept {
    const usize head = static_cast<usize>(range.head().idx);
    const usize len = static_cast<usize>(range.size());
    FPAG_DCHECK_LE(head, size_);
    FPAG_DCHECK_LE(len, size_ - head);
    return SooVecSlice(data_ + head, len);
  }

  [[nodiscard]] constexpr SooVecSlice first(usize count) const noexcept {
    FPAG_DCHECK_LE(count, size_);
    return SooVecSlice(data_, count);
  }

  [[nodiscard]] constexpr SooVecSlice last(usize count) const noexcept {
    FPAG_DCHECK_LE(count, size_);
    return SooVecSlice(data_ + (size_ - count), count);
  }

 private:
  pointer data_ = nullptr;
  usize size_ = 0;
};

// Type alias for ConstSooVecSlice
template <typename T, HasIdxType Idx>
using ConstSooVecSlice = SooVecSlice<const T, Idx>;

}  // namespace base
