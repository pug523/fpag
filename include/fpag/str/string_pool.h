// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstring>
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
//
// A name is stored as its length followed by its bytes, both 4-byte aligned,
// so an id is the offset of the bytes and the length is a load away from them:
// see StringPoolId for why the length is not in the id.
class StringPool {
 public:
  // Default reservation: generous on 64-bit, still addressable everywhere
  // (offsets must fit in StringPoolId's u32). Small on 32-bit address spaces
  // (e.g. wasm32): every StringInterner reserves this up front, so it must stay
  // well under linear-memory caps.
#if FPAG_BUILD_FLAG(IS_ARCH_64_BITS)
  static constexpr usize DEFAULT_POOL_CAPACITY = 1ull * 1024 * 1024 * 1024;
#else
  static constexpr usize DEFAULT_POOL_CAPACITY = 64ull * 1024 * 1024;
#endif

  explicit StringPool(usize capacity = DEFAULT_POOL_CAPACITY);
  ~StringPool() = default;

  StringPool(const StringPool&) = delete;
  StringPool& operator=(const StringPool&) = delete;

  StringPool(StringPool&& other) noexcept;
  StringPool& operator=(StringPool&& other) noexcept;

  StringPoolId append(const std::string_view str,
                      std::string_view* out = nullptr);

  std::string_view get(StringPoolId id) const {
    const char* const bytes = data(id);
    return {bytes, length_at(bytes)};
  }

  // The bytes @p id names, without reading the length in front of them. A
  // caller that walks the pool does its own length compare against what it has
  // in hand, and the base below is one load rather than two.
  const char* data(StringPoolId id) const { return base() + id.offset; }

  // What an offset in an id is relative to.
  const char* base() const { return arena_.base_ptr(); }

  // The length of the name whose bytes are at @p data, which the pool stores in
  // the bytes in front of them. Read through memcpy because the length sits
  // where the previous name ended, which is aligned only as far as that name's
  // own length left it.
  static u32 length_at(const char* data) {
    u32 length = 0;
    std::memcpy(&length, data - LENGTH_PREFIX_BYTES, sizeof(length));
    return length;
  }

  // Releases all strings and re-reserves the same capacity, so the pool
  // stays usable (e.g. interning a fresh compilation unit).
  void reset() {
    arena_.reset();
    arena_.reserve(capacity_);
    totals_.store(0, std::memory_order_relaxed);
    reserve_empty_name();
  }

  // Returns the total size of all strings in the pool.
  usize size() const {
    return static_cast<usize>(totals_.load(std::memory_order_relaxed) >>
                              BYTES_SHIFT);
  }
  // Returns the number of strings in the pool.
  usize string_count() const {
    return static_cast<usize>(totals_.load(std::memory_order_relaxed) &
                              STRING_COUNT_MASK);
  }
  // Returns the reserved capacity in bytes.
  usize capacity() const { return capacity_; }

 private:
  // Bytes and strings share one word, so an append is one read-modify-write on
  // the line every appender shares instead of two: at eight threads the second
  // update is a second trip through the coherence protocol for a number nothing
  // on the hot path reads. Both halves fit: the arena cannot hand out more than
  // capacity_ bytes, and capacity_ is capped at u32::max, and a name takes at
  // least LENGTH_PREFIX_BYTES + 1 bytes, so the count is far below the byte
  // count's own limit.
  static constexpr u32 BYTES_SHIFT = 32;
  static constexpr u64 STRING_COUNT_MASK = 0xFFFFFFFFull;

  // Claims the empty name's slot, so that offset zero holds a length of zero
  // rather than whatever a fresh mapping happens to read as.
  void reserve_empty_name();

  mem::ConcurrentArena arena_;
  usize capacity_ = 0;
  std::atomic<u64> totals_{0};
};

}  // namespace str
