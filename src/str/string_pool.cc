// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "str/string_pool.h"

#include <atomic>
#include <cstring>
#include <string_view>
#include <utility>

#include "mem/arena_allocator.h"
#include "str/string_pool_id.h"

namespace str {

StringPool::StringPool(StringPool&& other) noexcept {
  arena_allocator_ = std::move(other.arena_allocator_);
  use_huge_pages_ = other.use_huge_pages_;

  size_.store(other.size_.load(std::memory_order_relaxed),
              std::memory_order_relaxed);
  string_count_.store(other.string_count_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);

  other.size_.store(0, std::memory_order_relaxed);
  other.string_count_.store(0, std::memory_order_relaxed);
}

StringPool& StringPool::operator=(StringPool&& other) noexcept {
  if (this == &other) [[unlikely]] {
    return *this;
  }

  arena_allocator_ = std::move(other.arena_allocator_);

  use_huge_pages_ = other.use_huge_pages_;

  size_.store(other.size_.load(std::memory_order_relaxed),
              std::memory_order_relaxed);
  string_count_.store(other.string_count_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);

  other.size_.store(0, std::memory_order_relaxed);
  other.string_count_.store(0, std::memory_order_relaxed);

  return *this;
}

StringPoolId StringPool::append(const std::string_view str,
                                std::string_view* out) {
  if (str.empty()) {
    return {.block_id = 0, .offset = 0, .length = 0};
  }

  mem::ArenaAllocator::BlockPosition block_pos{};
  void* const ptr =
      arena_allocator_.alloc(str.size(), use_huge_pages_, 1, &block_pos);
  std::memcpy(ptr, str.data(), str.size());

  if (out) {
    *out = std::string_view(static_cast<char*>(ptr), str.size());
  }

  size_.fetch_add(str.size(), std::memory_order_relaxed);
  string_count_.fetch_add(1, std::memory_order_relaxed);

  return {.block_id = block_pos.block_id,
          .offset = block_pos.offset,
          .length = str.size()};
}

}  // namespace str
