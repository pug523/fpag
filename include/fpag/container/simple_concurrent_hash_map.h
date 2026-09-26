// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <functional>
#include <utility>

#include "fpag/base/limits.h"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/debug/check.h"
#include "fpag/debug/fatal.h"
#include "fpag/mem/page_allocator.h"

namespace container {
// Lock-free fast concurrent hash map.
// Has no resizing.
template <typename K, typename V, typename Hash = std::hash<K>>
class SimpleConcurrentHashMap {
 public:
  explicit SimpleConcurrentHashMap(u64 capacity = static_cast<u64>(1024 * 1024),
                                   const Hash& hasher = Hash())
      : hasher_(hasher) {
    reserve(capacity);
  }

  ~SimpleConcurrentHashMap() { reset(); }

  SimpleConcurrentHashMap(const SimpleConcurrentHashMap&) = delete;
  SimpleConcurrentHashMap& operator=(const SimpleConcurrentHashMap&) = delete;

  SimpleConcurrentHashMap(SimpleConcurrentHashMap&& other) noexcept
      : hasher_(std::move(other.hasher_)) {
    capacity_.store(other.capacity_.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
    size_.store(other.size_.load(std::memory_order_relaxed),
                std::memory_order_relaxed);

    entries_ = other.entries_;

    other.entries_ = nullptr;
    other.capacity_.store(0, std::memory_order_relaxed);
    other.size_.store(0, std::memory_order_relaxed);
  }

  SimpleConcurrentHashMap& operator=(SimpleConcurrentHashMap&& other) noexcept {
    if (this != &other) [[likely]] {
      reset();

      hasher_ = std::move(other.hasher_);
      capacity_.store(other.capacity_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
      size_.store(other.size_.load(std::memory_order_relaxed),
                  std::memory_order_relaxed);
      entries_ = other.entries_;

      other.entries_ = nullptr;
      other.capacity_.store(0, std::memory_order_relaxed);
      other.size_.store(0, std::memory_order_relaxed);
    }
    return *this;
  }

  // Releases the storage the map already holds and allocates one of
  // `capacity` entries, so a second reserve() does not leak the first. A
  // capacity of zero releases the storage and leaves the map without any,
  // which is what the construct-then-init path needs.
  void reserve(u64 capacity) {
    reset();

    if (capacity == 0) {
      return;
    }

    // A capacity that is not a power of two makes `& mask` wrong, and a
    // capacity that is too large overflows `sizeof(Entry) * capacity`, so both
    // would corrupt the map rather than fail.
    FPAG_CHECK_MSG(base::is_power_of_two(capacity),
                   "SimpleConcurrentHashMap: capacity must be a power of two");
    FPAG_CHECK_MSG(capacity <= kMaxCapacity,
                   "SimpleConcurrentHashMap: capacity is out of range");

    const u64 entries_capacity = base::round_up(capacity, mem::page_size());
    capacity_.store(entries_capacity, std::memory_order_relaxed);
    void* const raw_mem = mem::allocate_pages(sizeof(Entry) * entries_capacity);
    FPAG_CHECK_MSG(raw_mem,
                   "SimpleConcurrentHashMap: failed to allocate the entries");
    std::memset(raw_mem, 0, sizeof(Entry) * entries_capacity);
    entries_ = static_cast<Entry*>(raw_mem);
  }

  void reset() {
    if (entries_) {
      mem::free_pages(
          entries_, sizeof(Entry) * capacity_.load(std::memory_order_relaxed));
      entries_ = nullptr;
    }
    capacity_.store(0, std::memory_order_relaxed);
    size_.store(0, std::memory_order_relaxed);
  }

  void insert(const K& key, const V& value) {
    const u64 h = hash(key);
    const u64 mask = capacity_ - 1;

    // Linear probing -> CAS -> increment `size_` if successful.
    for (u64 i = 0; i < capacity_; ++i) {
      const u64 idx = (h + i) & mask;
      Entry& e = entries_[idx];

      u64 expected = kEmptyHash;

      // Check if the entry is empty
      if (e.hash.load(std::memory_order_acquire) == kEmptyHash) {
        // Try to lock the entry
        if (e.hash.compare_exchange_strong(expected, kLockedHash,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
          // We own the slot - write key/value, then publish the hash.
          e.key = key;
          e.value = value;
          e.hash.store(h, std::memory_order_release);
          size_.fetch_add(1, std::memory_order_relaxed);
          return;
        }
        // If compare_exchange failed, `expected` now holds the observed
        // value.
      }

      // Check if entry matches the key.
      if (e.hash.load(std::memory_order_acquire) == h && e.key == key) {
        // Already exists, update value.
        e.value = value;
        return;
      }
    }
  }

  const V* find(const K& key) const {
    const u64 h = hash(key);
    const u64 mask = capacity_ - 1;

    // Linear probing.
    for (u64 i = 0; i < capacity_; ++i) {
      const u64 idx = (h + i) & mask;
      const Entry& e = entries_[idx];
      const u64 entry_hash = e.hash.load(std::memory_order_acquire);
      if (entry_hash == h && e.key == key) [[likely]] {
        return &e.value;
      } else if (entry_hash == kEmptyHash) {
        return nullptr;
      }
    }
    return nullptr;
  }

  const V* try_insert(const K& key, const V& value, bool* inserted) {
    const u64 h = hash(key);
    const u64 mask = capacity_.load(std::memory_order_relaxed) - 1;

    for (u64 i = 0; i < capacity_.load(std::memory_order_relaxed); ++i) {
      const u64 idx = (h + i) & mask;
      Entry& e = entries_[idx];

      u64 cur = e.hash.load(std::memory_order_acquire);

      // Spin wait until the slot is unlocked.
      while (cur == kLockedHash) {
        cur = e.hash.load(std::memory_order_acquire);
      }

      if (cur == kEmptyHash) {
        u64 expected = kEmptyHash;
        if (e.hash.compare_exchange_strong(expected, kLockedHash,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
          // Successfully locked the slot; write the key/value and publish.
          e.key = key;
          e.value = value;
          e.hash.store(h, std::memory_order_release);
          size_.fetch_add(1, std::memory_order_relaxed);
          *inserted = true;
          return &e.value;
        }
        // Failed to lock the slot; retry this slot (do not advance to next).
        if (expected == kLockedHash) {
          --i;
        }
        continue;
      }

      // Found an existing entry with the same key; return it.
      if (cur == h && e.key == key) {
        *inserted = false;
        return &e.value;
      }

      // Hash collision with a different key; try the next slot.
    }

    // Full; should not happen.
    FPAG_UNREACHABLE();
  }

  u64 capacity() const { return capacity_.load(std::memory_order_relaxed); }
  u64 size() const { return size_.load(std::memory_order_relaxed); }

 private:
  struct alignas(std::max_align_t) Entry {
    std::atomic<u64> hash;
    K key;
    V value;
  };

  static constexpr u64 kEmptyHash = 0;
  static constexpr u64 kLockedHash = kU64Max;
  // The entries go to mem::allocate_pages() as one byte size, so the count has
  // to keep `sizeof(Entry) * capacity` representable.
  static constexpr u64 kMaxCapacity = kU64Max / sizeof(Entry);

  u64 hash(const K& key) const {
    const u64 h = hasher_(key);
    if (h == kEmptyHash || h == kLockedHash) [[unlikely]] {
      return 1;
    } else {
      return h;
    }
  }

  std::atomic<u64> capacity_ = 0;
  std::atomic<u64> size_ = 0;
  Entry* entries_ = nullptr;
  Hash hasher_;
};

}  // namespace container
