// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <utility>

#include "base/debug/check.h"
#include "base/numeric.h"
#include "mem/arena_ptr.h"

namespace mem {

class Arena {
 public:
  Arena() = default;
  ~Arena() {
    if (ptr_) {
      reset();
    }
  }

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  Arena(Arena&& other) noexcept;
  Arena& operator=(Arena&& other) noexcept;

  // Reserves the specified capacity.
  void reserve(usize capacity);

  // Frees all allocated memory and resets the arena to the uninitialized state.
  void reset();

  // Allocates committed memory of the specified size and alignment.
  [[nodiscard]] void* alloc(usize size,
                            usize align = alignof(std::max_align_t));

  void commit_until(usize end_offset);

  // Doesn't call destructor.
  // Use this for trivial copyable types.
  template <typename T, typename... Args>
  [[nodiscard]] inline T* create(Args&&... args) {
    void* mem = alloc(sizeof(T), alignof(T));
    T* const obj = new (mem) T(std::forward<Args>(args)...);
    FPAG_DCHECK(obj);
    return obj;
  }

  // Calls destructor automatically.
  template <typename T, typename... Args>
  [[nodiscard]] inline ArenaUniquePtr<T> create_managed(Args&&... args) {
    return ArenaUniquePtr<T>(create<T>(std::forward<Args>(args)...));
  }

  inline const char* base_ptr() const { return ptr_; }
  inline usize capacity() const { return capacity_; }
  inline usize size() const { return size_; }
  inline usize committed_size() const { return committed_size_; }

 private:
  char* ptr_ = nullptr;
  usize capacity_ = 0;
  usize size_ = 0;
  usize committed_size_ = 0;
};

}  // namespace mem
