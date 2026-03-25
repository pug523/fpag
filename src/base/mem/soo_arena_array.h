// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <utility>

#include "base/mem/blocked_arena_array.h"
#include "base/numeric.h"

namespace base {

template <typename T, usize kSooSize = 128, bool kUseHugePages = false>
class SooArenaArray {
 public:
  explicit SooArenaArray() : dynamic_array_(kUseHugePages) {}
  ~SooArenaArray() = default;

  SooArenaArray(const SooArenaArray&) = delete;
  SooArenaArray& operator=(const SooArenaArray&) = delete;

  SooArenaArray(SooArenaArray&&) noexcept = default;
  SooArenaArray& operator=(SooArenaArray&&) noexcept = default;

  template <typename... Args>
  usize emplace_back(Args&&... args) {
    if (!is_static_array_full()) [[likely]] {
      static_array_[current_idx_] = T(std::forward<Args>(args)...);
    } else {
      dynamic_array_.emplace_back(std::forward<Args>(args)...);
    }
    return current_idx_++;
  }

  usize push_back(T&& obj) {
    if (!is_static_array_full()) [[likely]] {
      static_array_[current_idx_] = std::move(obj);
    } else {
      dynamic_array_.emplace_back(std::move(obj));
    }
    return current_idx_++;
  }

  inline const T& at(usize index) const {
    dcheck(0 <= index && index < current_idx_);
    return index < kSooSize ? static_array_[index]
                            : dynamic_array_[index - kSooSize];
  }

  inline const T& operator[](usize index) const { return at(index); }

  usize size() const { return current_idx_; }

 private:
  // static_assert(Arena::is_power_of_two(kSooSize));

  inline bool is_static_array_full() const { return current_idx_ >= kSooSize; }

  T static_array_[kSooSize];
  ChunkedArenaArray<T> dynamic_array_;
  usize current_idx_ = 0;
};

}  // namespace base
