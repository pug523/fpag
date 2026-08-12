// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <type_traits>

#include "fpag/base/idx.h"
#include "fpag/debug/check.h"

namespace base {

template <HasIdxType Idx>
class IdxRange {
 public:
  using IdxType = typename Idx::IdxType;

  constexpr IdxRange() = default;

  constexpr IdxRange(Idx head, IdxType size) : head_(head), size_(size) {}
  constexpr ~IdxRange() = default;

  constexpr IdxRange(const IdxRange&) = default;
  constexpr IdxRange& operator=(const IdxRange&) = default;

  constexpr IdxRange(IdxRange&&) noexcept = default;
  constexpr IdxRange& operator=(IdxRange&&) noexcept = default;

  struct Iterator {
    IdxType value;

    Idx operator*() const { return Idx{value}; }
    Iterator& operator++() {
      ++value;
      return *this;
    }
    bool operator!=(const Iterator& other) const {
      return value != other.value;
    }
    bool operator==(const Iterator& other) const {
      return value == other.value;
    }
  };

  Iterator begin() const { return Iterator{static_cast<IdxType>(head_.idx)}; }
  Iterator end() const {
    return Iterator{static_cast<IdxType>(head_.idx) + size_};
  }

  Idx head() const { return head_; }
  IdxType size() const { return size_; }
  bool empty() const { return size() == 0; }

  template <typename N>
  Idx operator[](N offset) const {
    static_assert(std::is_integral_v<N>);
    if constexpr (std::is_signed_v<N>) {
      FPAG_DCHECK_LE(static_cast<N>(0), offset);
    }
    FPAG_DCHECK_LT(offset, static_cast<N>(size()));
    return head_ + offset;
  }

  static constexpr IdxRange<Idx> from_to(Idx from, Idx to) {
    return IdxRange<Idx>(from, static_cast<IdxType>(to.idx - from.idx + 1));
  }

 private:
  Idx head_ = Idx(0);
  IdxType size_ = 0;
};

}  // namespace base
