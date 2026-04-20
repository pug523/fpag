// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstddef>
#include <utility>

#include "fpag/base/debug/check.h"
#include "fpag/base/numeric.h"
#include "fpag/mem/arena_ptr.h"

namespace mem {

class ConcurrentArena {
 public:
  ConcurrentArena() = default;
  ~ConcurrentArena() {
    if (ptr_) {
      reset();
    }
  }

  ConcurrentArena(const ConcurrentArena&) = delete;
  ConcurrentArena& operator=(const ConcurrentArena&) = delete;

  ConcurrentArena(ConcurrentArena&& other) noexcept;
  ConcurrentArena& operator=(ConcurrentArena&& other) noexcept;

  // Not thread-safe (must be called before use).
  void reserve(usize capacity);

  // Not thread-safe.
  void reset();

  // Lock-free allocation.
  [[nodiscard]] void* alloc(usize size,
                            usize align = alignof(std::max_align_t));

  template <typename T, typename... Args>
  [[nodiscard]] inline T* create(Args&&... args) {
    void* mem = alloc(sizeof(T), alignof(T));
    T* const obj = new (mem) T(std::forward<Args>(args)...);
    FPAG_DCHECK(obj);
    return obj;
  }

  template <typename T, typename... Args>
  [[nodiscard]] inline ArenaUniquePtr<T> create_managed(Args&&... args) {
    return ArenaUniquePtr<T>(create<T>(std::forward<Args>(args)...));
  }

  inline const char* base_ptr() const { return ptr_; }
  inline usize capacity() const { return capacity_; }
  inline usize size() const { return size_.load(std::memory_order_relaxed); }
  inline usize committed_size() const {
    return committed_size_.load(std::memory_order_relaxed);
  }

 private:
  char* ptr_ = nullptr;
  usize capacity_ = 0;

  std::atomic<usize> size_{0};
  std::atomic<usize> committed_size_{0};
};

}  // namespace mem
