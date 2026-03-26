// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <string_view>

#include "base/mem/arena_allocator.h"
#include "base/mem/string_pool_id.h"
#include "base/numeric.h"

namespace base {

// Thread-safe string pool.
// Has auto resizing.
class StringPool {
 public:
  explicit StringPool(usize init_capacity, bool use_huge_pages = false) {
    use_huge_pages_ = use_huge_pages;
    arena_allocator_.reserve(init_capacity);
  }
  ~StringPool() = default;

  StringPool(const StringPool&) = delete;
  StringPool& operator=(const StringPool&) = delete;

  StringPool(StringPool&& other) noexcept;
  StringPool& operator=(StringPool&& other) noexcept;

  StringPoolId append(const std::string_view str,
                      std::string_view* out = nullptr);

  inline std::string_view get(StringPoolId id) const {
    const ArenaAllocator::Block* block_base =
        arena_allocator_.block(id.block_id);
    return {reinterpret_cast<const char*>(block_base) + id.offset, id.length};
  }

  inline void reserve(usize capacity) {
    arena_allocator_.reserve(capacity, use_huge_pages_);
  }
  inline void reset() {
    arena_allocator_.reset();
    size_.store(0, std::memory_order_relaxed);
    string_count_.store(0, std::memory_order_relaxed);
  }

  // Returns the total size of all strings in the pool.
  inline usize size() const { return size_; }

  // Returns the number of strings in the pool.
  inline usize string_count() const { return string_count_; }

  struct Checkpoint {
    ArenaAllocator::BlockPosition pos;
  };

  inline Checkpoint checkpoint() const {
    return {arena_allocator_.current_position()};
  }

  inline void rollback(const Checkpoint& cp) {
    arena_allocator_.rollback(cp.pos);
  }

 private:
  ArenaAllocator arena_allocator_;
  std::atomic<usize> size_ = 0;
  std::atomic<usize> string_count_ = 0;
  bool use_huge_pages_ = false;
};

}  // namespace base
