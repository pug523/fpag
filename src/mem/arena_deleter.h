// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <memory>

namespace mem {

template <typename T>
struct ArenaDeleter {
  void operator()(T* ptr) const {
    if (ptr) {
      ptr->~T();
    }
  }
};

template <typename T>
using ArenaUniquePtr = std::unique_ptr<T, ArenaDeleter<T>>;

}  // namespace mem
