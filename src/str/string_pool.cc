// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/str/string_pool.h"

#include <atomic>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

#include "fpag/base/numeric.h"
#include "fpag/debug/check.h"
#include "fpag/str/string_pool_id.h"

namespace str {

StringPool::StringPool(StringPool&& other) noexcept
    : capacity_(std::exchange(other.capacity_, 0)) {
  arena_ = std::move(other.arena_);

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

  arena_ = std::move(other.arena_);
  capacity_ = std::exchange(other.capacity_, 0);

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
    return kEmptyStringId;
  }

  const usize offset = arena_.size();
  void* const ptr = arena_.alloc(str.size(), 1);
  std::memcpy(ptr, str.data(), str.size());

  if (out) {
    *out = std::string_view(static_cast<char*>(ptr), str.size());
  }

  size_.fetch_add(str.size(), std::memory_order_relaxed);
  string_count_.fetch_add(1, std::memory_order_relaxed);

  // Offsets always fit: capacity_ is capped at u32 max at construction and
  // the arena refuses allocations past it.
  FPAG_DCHECK(offset <= static_cast<usize>(std::numeric_limits<u32>::max()));
  FPAG_DCHECK(str.size() <=
              static_cast<usize>(std::numeric_limits<u32>::max()));
  return {.offset = static_cast<u32>(offset),
          .length = static_cast<u32>(str.size())};
}

}  // namespace str
