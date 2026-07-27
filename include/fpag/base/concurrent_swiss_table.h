// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <functional>

#include "fpag/base/numeric.h"
#include "fpag/base/simd_control_group.h"

namespace base {

template <typename K, typename V, typename Hash = std::hash<K>>
class ConcurrentSwissTable {
 public:
  static constexpr u8 kCtrlEmpty = 0b10000000;   // 0x80
  static constexpr u8 kCtrlLocked = 0b11111111;  // 0xFF
  static constexpr u8 kCtrlH2Mask = 0b01111111;  // 0x7F

  struct Slot {
    K key{};
    V value{};
  };

  explicit ConcurrentSwissTable(usize capacity, const Hash& hasher = Hash())
      : hasher_(hasher) {
    capacity_ = 16;
    while (capacity_ < capacity) {
      capacity_ <<= 1;
    }
    capacity_mask_ = capacity_ - 1;

    const usize total_ctrl_size = capacity_ + SimdControlGroup::kGroupSize;
    ctrl_ = new std::atomic<u8>[total_ctrl_size];
    slots_ = new Slot[capacity_];

    for (usize i = 0; i < total_ctrl_size; ++i) {
      ctrl_[i].store(kCtrlEmpty, std::memory_order_relaxed);
    }
  }

  ~ConcurrentSwissTable() {
    delete[] ctrl_;
    delete[] slots_;
  }

  // Lock-free Find
  [[nodiscard]] bool find(const K& key, V* out_value) const {
    const u64 h = hasher_(key);
    const u8 target_h2 = extract_h2(h);
    usize group_idx = extract_h1(h) & capacity_mask_;

    for (usize probe = 0; probe < capacity_;
         probe += SimdControlGroup::kGroupSize) {
      u8 local_ctrl[SimdControlGroup::kGroupSize];

      for (usize i = 0; i < SimdControlGroup::kGroupSize; ++i) {
        local_ctrl[i] = ctrl_[group_idx + i].load(std::memory_order_relaxed);
      }

      // Wait if any slot in the current group is being updated.
      SimdControlGroup group(local_ctrl);
      while (group.match_locked(kCtrlLocked) != 0) {
        for (usize i = 0; i < SimdControlGroup::kGroupSize; ++i) {
          local_ctrl[i] = ctrl_[group_idx + i].load(std::memory_order_relaxed);
        }
        group = SimdControlGroup(local_ctrl);
      }

      std::atomic_thread_fence(std::memory_order_acquire);

      u32 match_mask = group.match_h2(target_h2);
      while (match_mask != 0) {
        const u32 bit_idx = static_cast<u32>(__builtin_ctz(match_mask));
        const usize slot_idx = (group_idx + bit_idx) & capacity_mask_;
        const Slot& slot = slots_[slot_idx];

        if (slot.key == key) {
          if (out_value != nullptr) {
            *out_value = slot.value;
          }
          return true;
        }
        match_mask &= match_mask - 1;
      }

      if (group.match_empty(kCtrlEmpty) != 0) {
        return false;
      }

      group_idx = (group_idx + SimdControlGroup::kGroupSize) & capacity_mask_;
    }
    return false;
  }

  // Thread-safe Insert (Deduplication + CAS Lock)
  bool insert(const K& key, const V& value) {
    const u64 h = hasher_(key);
    const u8 target_h2 = extract_h2(h);
    usize group_idx = extract_h1(h) & capacity_mask_;

    for (usize probe = 0; probe < capacity_;
         probe += SimdControlGroup::kGroupSize) {
      u8 local_ctrl[SimdControlGroup::kGroupSize];

      for (usize i = 0; i < SimdControlGroup::kGroupSize; ++i) {
        local_ctrl[i] = ctrl_[group_idx + i].load(std::memory_order_relaxed);
      }

      // Spin wait if locked
      SimdControlGroup group(local_ctrl);
      while (group.match_locked(kCtrlLocked) != 0) {
        for (usize i = 0; i < SimdControlGroup::kGroupSize; ++i) {
          local_ctrl[i] = ctrl_[group_idx + i].load(std::memory_order_relaxed);
        }
        group = SimdControlGroup(local_ctrl);
      }

      std::atomic_thread_fence(std::memory_order_acquire);

      // Check for existing key match
      u32 match_mask = group.match_h2(target_h2);
      while (match_mask != 0) {
        const u32 bit_idx = static_cast<u32>(__builtin_ctz(match_mask));
        const usize slot_idx = (group_idx + bit_idx) & capacity_mask_;

        if (slots_[slot_idx].key == key) {
          // Existing key found: do NOT overwrite.
          // slots_[slot_idx].value = value;
          return false;
        }
        match_mask &= match_mask - 1;
      }

      // Try to acquire empty slot
      const u32 empty_mask = group.match_empty(kCtrlEmpty);
      if (empty_mask != 0) {
        // Always target the first available empty slot.
        // All concurrent threads for the same key will compete for this exact
        // slot.
        const u32 bit_idx = static_cast<u32>(__builtin_ctz(empty_mask));
        const usize target_idx = group_idx + bit_idx;
        u8 expected = kCtrlEmpty;

        if (ctrl_[target_idx].compare_exchange_strong(
                expected, kCtrlLocked, std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
          const usize slot_idx = target_idx & capacity_mask_;

          // Write data while slot is locked
          slots_[slot_idx].key = key;
          slots_[slot_idx].value = value;

          // Publish control byte
          ctrl_[target_idx].store(target_h2, std::memory_order_release);
          if (slot_idx < SimdControlGroup::kGroupSize) {
            ctrl_[capacity_ + slot_idx].store(target_h2,
                                              std::memory_order_release);
          }

          size_.fetch_add(1, std::memory_order_relaxed);
          return true;
        }
        // Another thread just locked this slot (or changed its state).
        // It might be inserting the exact same key. We must retry the current
        // group to detect their lock, spin, and eventually find their inserted
        // key. By subtracting group size, the loop increment neutralizes this,
        // effectively retrying the same group_idx on the next iteration.
        probe -= SimdControlGroup::kGroupSize;
        continue;
      }

      group_idx = (group_idx + SimdControlGroup::kGroupSize) & capacity_mask_;
    }

    return false;
  }

 private:
  [[nodiscard]] static inline u64 extract_h1(u64 hash) { return hash >> 7; }
  [[nodiscard]] static inline u8 extract_h2(u64 hash) {
    return static_cast<u8>(hash & kCtrlH2Mask);
  }

  Hash hasher_;
  usize capacity_{0};
  usize capacity_mask_{0};
  std::atomic<usize> size_{0};
  std::atomic<u8>* ctrl_{nullptr};
  Slot* slots_{nullptr};
};

}  // namespace base
