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
class SooVectorSlice {
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

  constexpr SooVectorSlice() noexcept = default;
  constexpr SooVectorSlice(pointer data, usize size) noexcept
      : data_(data), size_(size) {}
  constexpr SooVectorSlice(pointer begin, pointer end) noexcept
      : data_(begin),
        size_(static_cast<usize>(end >= begin ? end - begin : 0)) {
    FPAG_DCHECK_GE(end, begin);
  }

  template <typename Container>
    requires(!std::is_same_v<std::decay_t<Container>, SooVectorSlice> &&
             requires(Container& c) {
               { c.data() } -> std::same_as<pointer>;
               { c.size() } -> std::convertible_to<usize>;
             })
  constexpr explicit SooVectorSlice(Container& container) noexcept
      : data_(container.data()), size_(container.size()) {}

  constexpr ~SooVectorSlice() = default;
  constexpr SooVectorSlice(const SooVectorSlice&) noexcept = default;
  constexpr SooVectorSlice& operator=(const SooVectorSlice&) noexcept = default;
  constexpr SooVectorSlice(SooVectorSlice&&) noexcept = default;
  constexpr SooVectorSlice& operator=(SooVectorSlice&&) noexcept = default;

  // Conversion from SooVectorSlice<T, Idx> to SooVectorSlice<const T, Idx>
  template <typename U = T>
    requires std::is_const_v<U>
  explicit constexpr SooVectorSlice(
      const SooVectorSlice<std::remove_const_t<T>, Idx>& other) noexcept
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

  [[nodiscard]] constexpr reference front() const noexcept {
    FPAG_DCHECK(!empty());
    return data_[0];
  }

  [[nodiscard]] constexpr reference back() const noexcept {
    FPAG_DCHECK(!empty());
    return data_[size_ - 1];
  }

  [[nodiscard]] constexpr IdxRange<Idx> idx_range() const noexcept {
    return IdxRange<Idx>(Idx{0}, static_cast<IdxType>(size_));
  }

  [[nodiscard]] constexpr SooVectorSlice subslice(usize offset,
                                                  usize count) const noexcept {
    FPAG_DCHECK_LE(offset, size_);
    FPAG_DCHECK_LE(count, size_ - offset);
    return SooVectorSlice(data_ + offset, count);
  }

  [[nodiscard]] constexpr SooVectorSlice subslice(
      IdxRange<Idx> range) const noexcept {
    const usize head = static_cast<usize>(range.head().idx);
    const usize len = static_cast<usize>(range.size());
    FPAG_DCHECK_LE(head, size_);
    FPAG_DCHECK_LE(len, size_ - head);
    return SooVectorSlice(data_ + head, len);
  }

  [[nodiscard]] constexpr SooVectorSlice first(usize count) const noexcept {
    FPAG_DCHECK_LE(count, size_);
    return SooVectorSlice(data_, count);
  }

  [[nodiscard]] constexpr SooVectorSlice last(usize count) const noexcept {
    FPAG_DCHECK_LE(count, size_);
    return SooVectorSlice(data_ + (size_ - count), count);
  }

 private:
  pointer data_ = nullptr;
  usize size_ = 0;
};

// Type alias for ConstSooVectorSlice
template <typename T, HasIdxType Idx>
using ConstSooVectorSlice = SooVectorSlice<const T, Idx>;

}  // namespace base
