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
    FPAG_CHECK_MSG(capacity <= MAX_CAPACITY,
                   "SimpleConcurrentHashMap: capacity is out of range");

    const u64 entries_capacity = base::round_up(capacity, mem::page_size());
    const u64 entries_size = sizeof(Entry) * entries_capacity;
    // Rounding the count up to a page can push the byte size past what usize
    // holds even when the count itself passed the check above.
    FPAG_CHECK_MSG(entries_size <= USIZE_MAX,
                   "SimpleConcurrentHashMap: capacity is out of range");
    capacity_.store(entries_capacity, std::memory_order_relaxed);
    void* const raw_mem = mem::allocate_pages(static_cast<usize>(entries_size));
    FPAG_CHECK_MSG(raw_mem,
                   "SimpleConcurrentHashMap: failed to allocate the entries");
    std::memset(raw_mem, 0, static_cast<usize>(entries_size));
    entries_ = static_cast<Entry*>(raw_mem);
  }

  void reset() {
    if (entries_) {
      mem::free_pages(entries_, static_cast<usize>(
                                    sizeof(Entry) *
                                    capacity_.load(std::memory_order_relaxed)));
      entries_ = nullptr;
    }
    capacity_.store(0, std::memory_order_relaxed);
    size_.store(0, std::memory_order_relaxed);
  }

  void insert(const K& key, const V& value) {
    FPAG_CHECK_MSG(entries_,
                   "SimpleConcurrentHashMap has no storage; call reserve()");
    const u64 h = hash(key);
    const u64 capacity = capacity_.load(std::memory_order_relaxed);
    const u64 mask = capacity - 1;

    // Linear probing -> CAS -> increment `size_` if successful.
    for (u64 i = 0; i < capacity; ++i) {
      const u64 idx = (h + i) & mask;
      Entry& e = entries_[idx];

      const u64 entry_hash = wait_unlocked(e);

      // Check if the entry is empty
      if (entry_hash == EMPTY_HASH) {
        u64 expected = EMPTY_HASH;
        // Try to lock the entry
        if (e.hash.compare_exchange_strong(expected, LOCKED_HASH,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
          // We own the slot - write key/value, then publish the hash.
          e.key = key;
          e.value = value;
          e.hash.store(h, std::memory_order_release);
          size_.fetch_add(1, std::memory_order_relaxed);
          return;
        }
        // Another thread took the slot while we looked at it. Look at the same
        // slot again: probing on would insert a second entry for a key that the
        // winner is in the middle of publishing.
        --i;
        continue;
      }

      // Check if entry matches the key.
      if (entry_hash == h && e.key == key) {
        // The key is already here. Take the slot before writing the value, or
        // the write races with every reader that got a pointer from find().
        u64 expected = h;
        if (e.hash.compare_exchange_strong(expected, LOCKED_HASH,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
          e.value = value;
          e.hash.store(h, std::memory_order_release);
          return;
        }
        --i;
        continue;
      }
    }

    // Full; should not happen.
    FPAG_UNREACHABLE();
  }

  const V* find(const K& key) const {
    const u64 h = hash(key);
    const u64 capacity = capacity_.load(std::memory_order_relaxed);
    const u64 mask = capacity - 1;

    // Linear probing.
    for (u64 i = 0; i < capacity; ++i) {
      const u64 idx = (h + i) & mask;
      const Entry& e = entries_[idx];
      const u64 entry_hash = wait_unlocked(e);
      if (entry_hash == h && e.key == key) [[likely]] {
        return &e.value;
      } else if (entry_hash == EMPTY_HASH) {
        return nullptr;
      }
    }
    return nullptr;
  }

  const V* try_insert(const K& key, const V& value, bool* inserted) {
    FPAG_CHECK_MSG(entries_,
                   "SimpleConcurrentHashMap has no storage; call reserve()");
    const u64 h = hash(key);
    const u64 capacity = capacity_.load(std::memory_order_relaxed);
    const u64 mask = capacity - 1;

    for (u64 i = 0; i < capacity; ++i) {
      const u64 idx = (h + i) & mask;
      Entry& e = entries_[idx];

      const u64 cur = wait_unlocked(e);

      if (cur == EMPTY_HASH) {
        u64 expected = EMPTY_HASH;
        if (e.hash.compare_exchange_strong(expected, LOCKED_HASH,
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
        // Failed to lock the slot; the winner may have published this very key,
        // so retry this slot instead of moving past it.
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

  static constexpr u64 EMPTY_HASH = 0;
  static constexpr u64 LOCKED_HASH = U64_MAX;
  // The entries go to mem::allocate_pages() as one byte size, so the count has
  // to keep `sizeof(Entry) * capacity` representable in usize.
  static constexpr u64 MAX_CAPACITY = USIZE_MAX / sizeof(Entry);

  // A slot is empty, locked by one writer, or published. Waits out a writer and
  // returns the state the slot settled on, so a caller that loses the race for
  // a slot re-reads that same slot instead of probing past a key that is being
  // inserted into it.
  static u64 wait_unlocked(const Entry& e) {
    u64 entry_hash = e.hash.load(std::memory_order_acquire);
    while (entry_hash == LOCKED_HASH) {
      entry_hash = e.hash.load(std::memory_order_acquire);
    }
    return entry_hash;
  }

  u64 hash(const K& key) const {
    const u64 h = hasher_(key);
    if (h == EMPTY_HASH || h == LOCKED_HASH) [[unlikely]] {
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
