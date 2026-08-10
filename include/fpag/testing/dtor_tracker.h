// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

namespace testing {

// Tracks destructor calls to ensure resource cleanup.
class DtorTracker {
 public:
  DtorTracker() = delete;
  explicit DtorTracker(bool* destroyed) noexcept : destroyed_(destroyed) {}

  DtorTracker(const DtorTracker&) = delete;
  DtorTracker& operator=(const DtorTracker&) = delete;

  DtorTracker(DtorTracker&& other) noexcept : destroyed_(other.destroyed_) {
    other.destroyed_ = nullptr;
  }

  DtorTracker& operator=(DtorTracker&& other) noexcept {
    if (this != &other) {
      if (destroyed_ != nullptr) {
        // Mark existing object as destroyed upon overwrite
        *destroyed_ = true;
      }
      destroyed_ = other.destroyed_;
      other.destroyed_ = nullptr;
    }
    return *this;
  }

  ~DtorTracker() noexcept {
    if (destroyed_ != nullptr) {
      *destroyed_ = true;
    }
  }

 private:
  bool* destroyed_{nullptr};
};

}  // namespace testing
