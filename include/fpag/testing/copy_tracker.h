// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace testing {

// Tracks copy construction/assignment operations.
struct CopyTracker {
  CopyTracker() = delete;
  explicit CopyTracker(i32* copy_count) noexcept : copy_count_(copy_count) {}

  CopyTracker(const CopyTracker& other) noexcept
      : copy_count_(other.copy_count_) {
    if (copy_count_ != nullptr) {
      (*copy_count_)++;
    }
  }

  CopyTracker& operator=(const CopyTracker& other) noexcept {
    if (this != &other) {
      copy_count_ = other.copy_count_;
      if (copy_count_ != nullptr) {
        (*copy_count_)++;
      }
    }
    return *this;
  }

  CopyTracker(CopyTracker&&) noexcept = default;
  CopyTracker& operator=(CopyTracker&&) noexcept = default;
  ~CopyTracker() noexcept = default;

  i32* copy_count_{nullptr};
};

}  // namespace testing
