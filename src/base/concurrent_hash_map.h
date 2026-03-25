// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <utility>

#include "base/debug/fatal.h"
#include "base/math_util.h"
#include "base/mem/page_allocator.h"
#include "base/numeric.h"

namespace base {

// Lock-free fast concurrent hash map.
// Has no resizing.
template <typename K, typename V, typename Hash = std::hash<K>>
class ConcurrentHashMap {
 public:
  explicit ConcurrentHashMap(u64 capacity = 1024 * 1024,
                             const Hash& hasher = Hash())
      : hasher_(hasher) {
    reserve(capacity);
  }

  ~ConcurrentHashMap() { reset(); }

  ConcurrentHashMap(const ConcurrentHashMap&) = delete;
  ConcurrentHashMap& operator=(const ConcurrentHashMap&) = delete;

  ConcurrentHashMap(ConcurrentHashMap&& other) noexcept
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

  ConcurrentHashMap& operator=(ConcurrentHashMap&& other) noexcept {
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

  inline void reserve(u64 capacity) {
    dcheck_msg(is_power_of_two(capacity), "capacity must be a power of two");
    capacity_ = capacity;
    void* const raw_mem = base::allocate_pages(sizeof(Entry) * capacity_);
    std::memset(raw_mem, 0, sizeof(Entry) * capacity_);
    entries_ = static_cast<Entry*>(raw_mem);
    dcheck(entries_);
  }

  inline void reset() {
    if (entries_) {
      base::free_pages(
          entries_, sizeof(Entry) * capacity_.load(std::memory_order_relaxed));
    }
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
        // If compare_exchange failed, `expected` now holds the observed value.
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
        --i;
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
    unreachable();
  }

  u64 capacity() const { return capacity_.load(std::memory_order_relaxed); }
  u64 size() const { return size_.load(std::memory_order_relaxed); }

 private:
  static constexpr u64 kEmptyHash = 0;
  static constexpr u64 kLockedHash = std::numeric_limits<u64>::max();

  inline u64 hash(const K& key) const {
    const u64 h = hasher_(key);
    if (h == kEmptyHash || h == kLockedHash) [[unlikely]] {
      return 1;
    } else {
      return h;
    }
  }

  struct alignas(std::max_align_t) Entry {
    std::atomic<u64> hash;
    K key;
    V value;
  };

  std::atomic<u64> capacity_ = 0;
  std::atomic<u64> size_ = 0;
  Entry* entries_ = nullptr;
  Hash hasher_;
};

}  // namespace base
