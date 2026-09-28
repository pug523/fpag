// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <string_view>
#include <thread>

#include "fpag/base/limits.h"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/debug/check.h"
#include "fpag/debug/fatal.h"
#include "fpag/hardware/cpu_yield.h"
#include "fpag/hash/xxh3_hasher.h"
#include "fpag/mem/page_allocator.h"
#include "fpag/str/string_pool.h"
#include "fpag/str/string_pool_id.h"

namespace str {

// The interner's table: a string's content maps to the pool id that holds it.
//
// Three properties shape it, and all three come from what an entry is. An entry
// is a StringPoolId and nothing else, because the id *is* the entry's content:
// an interner has no key and a value, it has a name and the one pool offset
// that holds it. So:
//
//   - An entry is 8 bytes and the control byte beside it is 1, so a name costs
//   9
//     bytes where a generic K/V entry costs 32, because a generic entry has to
//     store the key (a 16-byte string_view) beside the value that duplicates
//     it. A table of a million names is 9 MiB rather than 32.
//   - Nothing is written to make a slot free. EMPTY is zero, and a fresh
//     anonymous mapping reads as zero, so sizing the table is a mapping and not
//     a mapping plus a memset over all of it. The pages of the tail that nobody
//     has reached stay uncommitted.
//   - The entry a lookup returns points into a region that is never reallocated
//     while the table lives, so it stays valid for as long as the interner
//     does, with no generation counter and no retry in the reader. A table that
//     grows by reallocating cannot promise that, which is why this one does not
//     grow: it is sized, and a caller that outgrows it gets a check that names
//     the number rather than a probe that walks off the end.
//
// A probe walks the control bytes rather than the entries, and the two are
// interleaved in one line: eight control bytes are eight bytes, so a name that
// is not there costs one line and no entry read at all. A published control
// byte carries a seven-bit fingerprint of the name, so the common case - a slot
// holding some other name - is one comparison against a byte the fingerprint
// already rejects, and the entry behind it is never loaded.
//
// There is no platform code here and no intrinsics, because the work is a byte
// load and a compare: this compiles and runs the same on the 32-bit targets,
// where a 64-bit atomic word wide group would not be lock-free.
template <typename Hash = hash::Xxh3Hasher64>
class InternTable {
 public:
  // How many names a default-sized table holds. Proportioned like
  // StringPool's default reservation and for the same reason: a caller that
  // does not know how many names it will intern should not have to say, and a
  // reservation costs address space rather than memory.
#if FPAG_BUILD_FLAG(IS_ARCH_64_BITS)
  static constexpr usize DEFAULT_NAMES = 1ull << 24;  // 16.7M names, 144 MiB
#else
  static constexpr usize DEFAULT_NAMES = 1ull << 20;  // 1M names, 9 MiB
#endif

  // @p pool is where an interned name is stored and where a probe compares a
  // candidate against, and has to outlive the table; the interner's member
  // order is what makes that true.
  explicit InternTable(StringPool* pool, usize names = 0) : pool_(pool) {
    static_assert(std::atomic_ref<u8>::is_always_lock_free,
                  "InternTable needs lock-free single-byte atomics");
    static_assert(sizeof(StringPoolId) == 8,
                  "InternTable's entry is a StringPoolId and nothing else");
    reserve(names);
  }

  ~InternTable() { release(); }

  InternTable(const InternTable&) = delete;
  InternTable& operator=(const InternTable&) = delete;
  InternTable(InternTable&&) = delete;
  InternTable& operator=(InternTable&&) = delete;

  // The id for @p content, appending it to the pool if it is not here yet.
  //
  // Two threads interning one name is the case this is shaped around. The first
  // to claim a slot holds that claim across the append, so the second finds the
  // winner's entry instead of appending a copy of its own: the pool pays once
  // for the name rather than once per thread that wanted it.
  StringPoolId intern(std::string_view content) {
    const u64 hash = hash_(content);
    for (;;) {
      const Probe probe = lookup(hash, content);
      if (probe.found != nullptr) {
        return *probe.found;
      }
      if (claim(probe.free_slot)) {
        const StringPoolId id = pool_->append(content);
        entries_[probe.free_slot] = id;
        // The entry is a plain store and the control byte is the release, so a
        // reader that sees this byte is guaranteed to see the entry.
        control(probe.free_slot)
            .store(published(hash), std::memory_order_release);
        count_.fetch_add(1, std::memory_order_relaxed);
        return id;
      }
      // Another writer claimed that slot, and it may be publishing this very
      // name. So the answer is in that slot or nowhere: look again rather than
      // walking past a name that is on its way in.
    }
  }

  // The id for @p content, or nullptr if it is not interned.
  const StringPoolId* find(std::string_view content) const {
    const Probe probe = lookup(hash_(content), content);
    return probe.found;
  }

  // How many names are interned, which is below the slot count: the slots are a
  // power of two above the names asked for, and the rest is the room that keeps
  // a probe short.
  usize count() const { return count_.load(std::memory_order_relaxed); }

  // The slot count, which is what the region was sized for.
  usize capacity() const { return slots_; }

  // What the region occupies, which is nine bytes a slot.
  usize allocated_bytes() const { return region_bytes_; }

  // Sizes the table for @p names, releasing the region it had. The ids a table
  // hands out are offsets into the pool and not into the region, so a caller
  // holding one is unaffected; what a caller cannot do is probe the table while
  // it is resized, and the interner's init() runs before its first intern.
  void reserve(usize names) {
    release();
    const usize wanted = names == 0 ? DEFAULT_NAMES : names;
    FPAG_CHECK_MSG(wanted <= MAX_NAMES,
                   "InternTable: more names than one table can hold");
    // A power of two above the names at a load factor that keeps a linear probe
    // to a slot or two. The alternative, a table that is nearly full, turns
    // every lookup into a scan of the table.
    const usize slots =
        base::next_power_of_two((wanted / LOAD_DENOMINATOR) * LOAD_NUMERATOR);
    const usize control_bytes = slots;
    const usize entry_bytes = slots * sizeof(StringPoolId);
    FPAG_CHECK_MSG(entry_bytes <= USIZE_MAX - control_bytes,
                   "InternTable: the table's size is out of range");

    const usize region_bytes =
        base::round_up(control_bytes + entry_bytes, mem::page_size());
    void* const region = mem::allocate_pages(region_bytes);
    FPAG_CHECK_MSG(region != nullptr,
                   "InternTable: the region could not be reserved");

    region_ = static_cast<u8*>(region);
    region_bytes_ = region_bytes;
    entries_ = reinterpret_cast<StringPoolId*>(region_ + control_bytes);
    slots_ = slots;
    // The mapping is what makes EMPTY free: an anonymous mapping the kernel
    // hands over reads as zero, and nothing has to be written to make that
    // true. Checked rather than assumed, because every slot in the table
    // depends on it.
    FPAG_DCHECK_MSG(region_[0] == EMPTY_CONTROL && entries_[0].offset == 0 &&
                        entries_[slots_ - 1].offset == 0,
                    "InternTable needs a mapping that reads as zero");
  }

 private:
  // A slot is free, or a writer is publishing into it, or it is published and
  // carries the fingerprint. A published byte has the high bit set and neither
  // sentinel does, so "is this slot occupied" is one test and a fingerprint
  // cannot collide with a sentinel.
  static constexpr u8 EMPTY_CONTROL = 0x00;
  static constexpr u8 CLAIMED_CONTROL = 0x7F;
  static constexpr u8 PUBLISHED_BIT = 0x80;

  static constexpr usize LOAD_NUMERATOR = 4;
  static constexpr usize LOAD_DENOMINATOR = 3;
  // A slot costs a control byte and an entry, and the table holds four slots
  // for every three names, so this is the largest name count whose size
  // arithmetic cannot overflow.
  static constexpr usize MAX_NAMES =
      USIZE_MAX / ((1 + sizeof(StringPoolId)) * LOAD_NUMERATOR);

  // A claim is held across a compare-exchange in the pool and a memcpy, so the
  // wait is short unless the writer is descheduled, which yielding is cheaper
  // than spinning on.
  static constexpr u32 SPINS_BEFORE_YIELD = 64;

  // What one probe found: the entry, or the slot to intern into. Exactly one of
  // the two is set, and asking for both in one walk is what keeps a miss to a
  // single pass over the control bytes.
  struct Probe {
    const StringPoolId* found;
    u64 free_slot;
  };

  static u8 published(u64 hash) {
    return static_cast<u8>((hash >> 56) & 0x7F) | PUBLISHED_BIT;
  }

  std::atomic_ref<u8> control(u64 index) const {
    return std::atomic_ref<u8>(region_[index & (slots_ - 1)]);
  }

  // Waits out a writer publishing into a slot, and returns the state the slot
  // settled on.
  u8 wait_out(u64 index) const {
    u32 spins = 0;
    for (;;) {
      const u8 state = control(index).load(std::memory_order_acquire);
      if (state != CLAIMED_CONTROL) {
        return state;
      }
      if (++spins < SPINS_BEFORE_YIELD) {
        hardware::cpu_yield();
      } else {
        std::this_thread::yield();
      }
    }
  }

  // Walks the probe sequence for @p hash and reports what is at the end of it.
  //
  // Linear probing stops at the first free slot, so a name can only be further
  // along the sequence than a free slot if it was never interned. That is what
  // makes one walk enough: the first free slot is the slot to intern into, and
  // a writer in the way is waited out, because the name this caller wants may
  // be the one being published.
  Probe lookup(u64 hash, std::string_view content) const {
    const u64 mask = slots_ - 1;
    const u64 start = hash & mask;
    const u8 fingerprint = published(hash);
    u64 index = start;
    for (u64 step = 0; step < slots_; ++step) {
      const u8 state = wait_out(index);
      if (state == EMPTY_CONTROL) {
        return {nullptr, index};
      }
      if (state == fingerprint) {
        const StringPoolId& entry = entries_[index];
        // A matching fingerprint is a candidate and not an answer: one bit in
        // seven is not a hash of the name, and a name that lost the race to be
        // interned first sits at an offset of its own.
        if (content == pool_->get(entry)) {
          return {&entry, 0};
        }
      }
      index = (index + 1) & mask;
    }
    // A name with no free slot is a caller that interned more names than the
    // table was sized for, which is reachable rather than impossible, so it is
    // reported as such. FPAG_CHECK would print "Expected: 'false'" for it.
    FPAG_UNREACHABLE_MSG(
        "InternTable: the table is full; size it for the names it holds "
        "(InternTable::reserve).");
  }

  // Takes a free slot for one writer. False means another writer took it, and
  // the caller has to look again.
  bool claim(u64 index) const {
    u8 expected = EMPTY_CONTROL;
    return control(index).compare_exchange_strong(expected, CLAIMED_CONTROL,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_relaxed);
  }

  void release() {
    if (region_ != nullptr) {
      mem::free_pages(region_, region_bytes_);
    }
    region_ = nullptr;
    entries_ = nullptr;
    slots_ = 0;
    region_bytes_ = 0;
    count_.store(0, std::memory_order_relaxed);
  }

  StringPool* pool_;
  u8* region_ = nullptr;
  StringPoolId* entries_ = nullptr;
  usize slots_ = 0;
  usize region_bytes_ = 0;
  std::atomic<usize> count_{0};
  Hash hash_{};
};

}  // namespace str
