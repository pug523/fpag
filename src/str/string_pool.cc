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
#include "fpag/mem/page_allocator.h"
#include "fpag/str/string_pool_id.h"

namespace str {

StringPool::StringPool(usize capacity) {
  // A capacity this pool cannot honour is checked in every build, not only
  // in a debug one: a capacity past u32 max truncates an offset into a
  // StringPoolId that names a different string, and an unaligned one is
  // refused by reserve_pages() with a message that names no parameter.
  FPAG_CHECK_MSG(capacity > 0, "Pool capacity must be nonzero.");
  FPAG_CHECK_MSG(
      capacity <= static_cast<usize>(std::numeric_limits<u32>::max()),
      "Pool capacity must fit in StringPoolId's u32 offset.");
  FPAG_CHECK_MSG(mem::is_page_aligned_size(capacity),
                 "Capacity must be page aligned.");
  capacity_ = capacity;
  arena_.reserve(capacity_);
  reserve_empty_name();
}

StringPool::StringPool(StringPool&& other) noexcept
    : arena_(std::move(other.arena_)),
      capacity_(std::exchange(other.capacity_, 0)),
      totals_(other.totals_.load(std::memory_order_relaxed)) {
  other.totals_.store(0, std::memory_order_relaxed);
}

StringPool& StringPool::operator=(StringPool&& other) noexcept {
  if (this != &other) [[unlikely]] {
    arena_ = std::move(other.arena_);
    capacity_ = std::exchange(other.capacity_, 0);
    totals_.store(other.totals_.load(std::memory_order_relaxed),
                  std::memory_order_relaxed);
    other.totals_.store(0, std::memory_order_relaxed);
  }

  return *this;
}

void StringPool::reserve_empty_name() {
  void* const ptr = arena_.alloc(LENGTH_PREFIX_BYTES, alignof(u32));
  FPAG_CHECK_MSG(ptr != nullptr, "StringPool is out of capacity.");

  const u32 empty = 0;
  std::memcpy(ptr, &empty, sizeof(empty));
}

StringPoolId StringPool::append(const std::string_view str,
                                std::string_view* out) {
  if (str.empty()) [[unlikely]] {
    return EMPTY_STRING_ID;
  }

  // The length goes in front of the bytes, so it is a load away from the first
  // byte of the name. The allocation is 4-byte aligned rather than byte aligned
  // for the same reason: the length is read on every comparison against a name
  // the interner already holds, and an aligned load of it is never split.
  void* const ptr =
      arena_.alloc(LENGTH_PREFIX_BYTES + str.size(), alignof(u32));
  FPAG_CHECK_MSG(ptr != nullptr, "StringPool is out of capacity.");

  const u32 length = static_cast<u32>(str.size());
  std::memcpy(ptr, &length, sizeof(length));
  char* const bytes = static_cast<char*>(ptr) + LENGTH_PREFIX_BYTES;
  std::memcpy(bytes, str.data(), str.size());

  if (out) {
    *out = {bytes, str.size()};
  }

  // The offset has to come from the pointer the arena handed out: reading the
  // arena's size before allocating races with the other appenders, and the id
  // would then name whichever string took that slot.
  const usize offset =
      static_cast<usize>(static_cast<const char*>(ptr) - arena_.base_ptr()) +
      LENGTH_PREFIX_BYTES;

  totals_.fetch_add((static_cast<u64>(str.size()) << BYTES_SHIFT) | 1,
                    std::memory_order_relaxed);

  // Offsets always fit: capacity_ is capped at u32 max at construction and
  // the arena refuses allocations past it.
  FPAG_DCHECK(offset <= static_cast<usize>(std::numeric_limits<u32>::max()));
  FPAG_DCHECK(str.size() <=
              static_cast<usize>(std::numeric_limits<u32>::max()));
  return {.offset = static_cast<u32>(offset)};
}

}  // namespace str
