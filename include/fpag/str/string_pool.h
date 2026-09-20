// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <limits>
#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/debug/check.h"
#include "fpag/mem/concurrent_arena.h"
#include "fpag/mem/page_allocator.h"
#include "fpag/str/string_pool_id.h"

namespace str {

// Thread-safe bump-allocated string pool.
//
// Storage is virtual address reservation up front (single PROT_NONE region)
// with physical pages committed on demand as strings are appended, so a
// large capacity costs no memory until used. Capacity is fixed at
// construction; there is no growth beyond it.
class StringPool {
 public:
  // Default reservation: generous on 64-bit, still addressable everywhere
  // (offsets must fit in StringPoolId's u32 fields). Small on 32-bit
  // address spaces (e.g. wasm32): every StringInterner reserves this up
  // front, so it must stay well under linear-memory caps.
#if FPAG_BUILD_FLAG(IS_ARCH_64_BITS)
  static constexpr usize kDefaultPoolCapacity = 1ull * 1024 * 1024 * 1024;
#else
  static constexpr usize kDefaultPoolCapacity = 64ull * 1024 * 1024;
#endif

  explicit StringPool(usize capacity = kDefaultPoolCapacity) {
    FPAG_DCHECK_MSG(capacity > 0, "Pool capacity must be nonzero.");
    FPAG_DCHECK_MSG(
        capacity <= static_cast<usize>(std::numeric_limits<u32>::max()),
        "Pool capacity must fit in StringPoolId's u32 offset.");
    FPAG_DCHECK_MSG(mem::is_page_aligned_size(capacity),
                    "Pool capacity must be page aligned.");
    capacity_ = capacity;
    arena_.reserve(capacity_);
  }
  ~StringPool() = default;

  StringPool(const StringPool&) = delete;
  StringPool& operator=(const StringPool&) = delete;

  StringPool(StringPool&& other) noexcept;
  StringPool& operator=(StringPool&& other) noexcept;

  StringPoolId append(const std::string_view str,
                      std::string_view* out = nullptr);

  std::string_view get(StringPoolId id) const {
    return {reinterpret_cast<const char*>(arena_.base_ptr()) + id.offset,
            id.length};
  }

  // Releases all strings and re-reserves the same capacity, so the pool
  // stays usable (e.g. interning a fresh compilation unit).
  void reset() {
    arena_.reset();
    arena_.reserve(capacity_);
    size_.store(0, std::memory_order_relaxed);
    string_count_.store(0, std::memory_order_relaxed);
  }

  // Returns the total size of all strings in the pool.
  usize size() const { return size_; }
  // Returns the number of strings in the pool.
  usize string_count() const { return string_count_; }
  // Returns the reserved capacity in bytes.
  usize capacity() const { return capacity_; }

 private:
  mem::ConcurrentArena arena_;
  usize capacity_ = 0;
  std::atomic<usize> size_ = 0;
  std::atomic<usize> string_count_ = 0;
};

}  // namespace str
