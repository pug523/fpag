// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace testing {

// Tracks move construction/assignment operations. (move-only)
class MoveTracker {
 public:
  MoveTracker() = delete;
  explicit MoveTracker(i32* move_count) noexcept : move_count_(move_count) {}

  MoveTracker(const MoveTracker&) = delete;
  MoveTracker& operator=(const MoveTracker&) = delete;

  MoveTracker(MoveTracker&& other) noexcept : move_count_(other.move_count_) {
    if (move_count_ != nullptr) {
      (*move_count_)++;
    }
    other.move_count_ = nullptr;
  }

  MoveTracker& operator=(MoveTracker&& other) noexcept {
    if (this != &other) {
      move_count_ = other.move_count_;
      if (move_count_ != nullptr) {
        (*move_count_)++;
      }
      other.move_count_ = nullptr;
    }
    return *this;
  }

  ~MoveTracker() noexcept = default;

 private:
  i32* move_count_{nullptr};
};

}  // namespace testing

