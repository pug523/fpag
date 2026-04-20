// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/mem/concurrent_arena.h"
#include "fpag/str/string_pool_id.h"

namespace str {

// Thread-safe string pool.
// Has auto resizing.
class StringPool {
 public:
  StringPool() { arena_.reserve(kMaxStringPoolCapacity); }
  ~StringPool() = default;

  StringPool(const StringPool&) = delete;
  StringPool& operator=(const StringPool&) = delete;

  StringPool(StringPool&& other) noexcept;
  StringPool& operator=(StringPool&& other) noexcept;

  StringPoolId append(const std::string_view str,
                      std::string_view* out = nullptr);

  inline std::string_view get(StringPoolId id) const {
    return {reinterpret_cast<const char*>(arena_.base_ptr()) + id.offset,
            id.length};
  }

  inline void reset() {
    arena_.reset();
    size_.store(0, std::memory_order_relaxed);
    string_count_.store(0, std::memory_order_relaxed);
  }

  // Returns the total size of all strings in the pool.
  inline usize size() const { return size_; }

  // Returns the number of strings in the pool.
  inline usize string_count() const { return string_count_; }

  static constexpr usize kMaxStringPoolCapacity = 64ull * 1024 * 1024 * 1024;

 private:
  mem::ConcurrentArena arena_;
  std::atomic<usize> size_ = 0;
  std::atomic<usize> string_count_ = 0;
};

}  // namespace str
