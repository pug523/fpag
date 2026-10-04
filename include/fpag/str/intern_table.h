// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstring>
#include <optional>
#include <string_view>

#include "fpag/base/attributes.h"
#include "fpag/base/limits.h"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/debug/check.h"
#include "fpag/debug/fatal.h"
#include "fpag/hardware/cpu_yield.h"
#include "fpag/hash/xxh3_hasher.h"
#include "fpag/mem/cache.h"
#include "fpag/mem/page_allocator.h"
#include "fpag/str/string_pool.h"
#include "fpag/str/string_pool_id.h"

namespace str {

// The interner's table: a string's content maps to the pool id that holds it.
//
// Four properties shape it, and all four come from what an entry is. An entry
// is a StringPoolId and nothing else, because the id *is* the entry's content:
// an interner has no key and a value, it has a name and the one pool offset
// that holds it. So:
//
//   - An entry is 4 bytes and the control byte beside it is 1, so a name costs
//     5 bytes where a generic K/V entry costs 32, because a generic entry has
//     to store the key (a 16-byte string_view) beside the value that duplicates
//     it. A table of a million names is 5 MiB rather than 32. The id carries no
//     length, so there is nothing in the entry but the offset.
//   - A control byte and the entry it publishes are adjacent rather than in two
//     arrays, so a probe that has found its entry has already loaded the line
//     the control byte was in. A lookup is two dependent memory accesses, the
//     pool's copy of the name being the second, where two arrays make it three.
//   - Nothing is written to make a slot free. EMPTY is zero, and a fresh
//     anonymous mapping reads as zero, so sizing the table is a mapping and not
//     a mapping plus a memset over all of it. The pages of the tail that nobody
//     has reached stay uncommitted.
//   - The id a lookup returns is an offset into the pool and not into the
//     region, so it stays valid for as long as the interner does, with no
//     generation counter and no retry in the reader. A table that grows by
//     reallocating cannot promise that, which is why this one does not grow: it
//     is sized, and a caller that outgrows it gets a check that names the
//     number rather than a probe that walks off the end.
//
// A probe walks the control bytes rather than the entries, and the two are in
// one line: a published control byte carries a seven-bit fingerprint of the
// name, so the common case - a slot holding some other name - is one comparison
// against a byte the fingerprint already rejects, and the entry behind it is
// never loaded.
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
  static constexpr usize DEFAULT_NAMES = 1ull << 24;  // 16.7M names, 160 MiB
#else
  static constexpr usize DEFAULT_NAMES = 1ull << 20;  // 1M names, 10 MiB
#endif

  // @p pool is where an interned name is stored and where a probe compares a
  // candidate against, and has to outlive the table; the interner's member
  // order is what makes that true.
  explicit InternTable(StringPool* pool, usize names = 0) : pool_(pool) {
    static_assert(std::atomic_ref<u8>::is_always_lock_free,
                  "InternTable needs lock-free single-byte atomics");
    static_assert(sizeof(StringPoolId) == 4,
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
    const std::optional<StringPoolId> id = try_intern(content);
    if (id.has_value()) {
      return *id;
    }
    // A name with no free slot is a caller that interned more names than the
    // table was sized for. FPAG_CHECK would print "Expected: 'false'" for it.
    FPAG_UNREACHABLE_MSG(
        "InternTable: the table is full; size it for the names it holds "
        "(InternTable::reserve).");
  }

  // The id for @p content, or nothing when the table has no free slot.
  //
  // A caller that can answer exhaustion - by reporting it, by growing, or by
  // refusing the input - uses this rather than intern, which treats the same
  // state as a programming error.
  std::optional<StringPoolId> try_intern(std::string_view content) {
    const u64 hash = hash_(content);
    const u8 fingerprint = published(hash);
    u8* const region = region_;
    u32 blocked = 0;
    for (;;) {
      const Probe probe = lookup(region, hash, fingerprint, content);
      if (probe.found) {
        return probe.id;
      }
      if (probe.full) {
        // Every slot is taken and a claim in flight only publishes into one,
        // so waiting cannot free a slot.
        return std::nullopt;
      }
      if (!probe.blocked && claim(region, probe.index)) {
        const StringPoolId id = pool_->append(content);
        write_entry(region, probe.index, id);
        // The entry is a plain store and the control byte is the release, so a
        // reader that sees this byte is guaranteed to see the entry and the
        // name behind it.
        control(region, probe.index)
            .store(fingerprint, std::memory_order_release);
        count_.fetch_add(1, std::memory_order_relaxed);
        return id;
      }
      // Another writer holds the slot this one would have taken, or took it
      // first, or is publishing the very name this call is here for. The answer
      // is in that slot or nowhere, so walk the sequence again rather than past
      // it.
      if (++blocked == SPINS_BEFORE_YIELD) {
        blocked = 0;
        hardware::cpu_yield();
      }
    }
  }

  // The id for @p content, or nothing if it is not interned.
  //
  // A lookup never waits for a writer. A slot a writer is publishing into holds
  // no published name yet, so walking past it is an answer that was true at the
  // moment it looked, and a lookup that a writer could stall is not a lookup.
  std::optional<StringPoolId> find(std::string_view content) const {
    const u64 hash = hash_(content);
    const Probe probe = lookup(region_, hash, published(hash), content);
    if (probe.found) {
      return probe.id;
    }
    return std::nullopt;
  }

  // How many names are interned, which is below the slot count: the slots are a
  // power of two above the names asked for, and the rest is the room that keeps
  // a probe short.
  usize count() const { return count_.load(std::memory_order_relaxed); }

  // The slot count, which is what the region was sized for.
  usize capacity() const { return slots_; }

  // What the region occupies, which is five bytes a slot.
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
    FPAG_DCHECK_MSG(slots <= static_cast<usize>(MAX_SLOTS),
                    "InternTable: the table's size is out of range");
    FPAG_CHECK_MSG(slots <= USIZE_MAX / SLOT_BYTES,
                   "InternTable: the table's size is out of range");

    const usize region_bytes =
        base::round_up(slots * SLOT_BYTES, mem::page_size());
    void* const region = mem::allocate_pages(region_bytes);
    FPAG_CHECK_MSG(region != nullptr,
                   "InternTable: the region could not be reserved");

    region_ = static_cast<u8*>(region);
    region_bytes_ = region_bytes;
    slots_ = slots;
    // The mapping is what makes a slot free: an anonymous mapping the kernel
    // hands over reads as zero, and nothing has to be written to make that
    // true. Checked rather than assumed, because every slot in the table
    // depends on it.
    FPAG_DCHECK_MSG(region_[0] == EMPTY_CONTROL &&
                        entry(region_, static_cast<u32>(slots - 1)).offset == 0,
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

  // A control byte and the entry it publishes, adjacent so that a probe which
  // has loaded the byte has the entry in the same line.
  static constexpr usize SLOT_BYTES = sizeof(StringPoolId) + 1;

  static constexpr usize LOAD_NUMERATOR = 4;
  static constexpr usize LOAD_DENOMINATOR = 3;

  // A slot's index travels in a u32 and a slot is SLOT_BYTES long, so this is
  // the largest table that both of those hold: a billion slots is five
  // gigabytes of address space, which is a reservation and not an allocation.
  static constexpr u32 MAX_SLOTS = 1u << 30;
  static constexpr usize MAX_NAMES =
      static_cast<usize>(MAX_SLOTS) / LOAD_NUMERATOR * LOAD_DENOMINATOR;

  // A claim is held across a compare-exchange in the pool and a memcpy, so the
  // wait is short unless the writer is descheduled, which yielding is cheaper
  // than spinning on.
  static constexpr u32 SPINS_BEFORE_YIELD = 64;

  // What one walk found: the name, or the slot to intern into and whether a
  // writer has to be given time first. Asking for all of it in one walk is what
  // keeps a miss to a single pass over the slots.
  struct Probe {
    StringPoolId id;
    u32 index;
    bool found;
    bool blocked;
    // The walk covered every slot without meeting this name or an empty one, so
    // the name cannot be interned into this table.
    bool full = false;
  };

  static u8 published(u64 hash) {
    return static_cast<u8>((hash >> 56) & 0x7F) | PUBLISHED_BIT;
  }

  static u64 word_at(const char* at) {
    u64 word;
    std::memcpy(&word, at, sizeof(word));
    return word;
  }

  // Whether @p length bytes at @p lhs and @p rhs are the same.
  //
  // A candidate is compared against a name whose length the caller already has,
  // and comparing two string_views is a call into memcmp. At the lengths a
  // compiler's names have, that call and the callee's own dispatch cost more
  // than the comparisons do: memcmp was a fifth of the instructions a lookup
  // issued. Reading nothing outside the name is a call's problem too, so the
  // tail is walked a byte at a time below eight bytes and read as an
  // overlapping word at or above it.
  static bool same_name(const char* lhs, const char* rhs, usize length) {
    usize at = 0;
    for (; at + sizeof(u64) <= length; at += sizeof(u64)) {
      if (word_at(lhs + at) != word_at(rhs + at)) {
        return false;
      }
    }
    if (at == length) {
      return true;
    }
    if (length >= sizeof(u64)) {
      return word_at(lhs + length - sizeof(u64)) ==
             word_at(rhs + length - sizeof(u64));
    }
    for (; at < length; ++at) {
      if (lhs[at] != rhs[at]) {
        return false;
      }
    }
    return true;
  }

  // The region is passed in rather than read from the table because a store
  // through it is a store of an unsigned char, which a compiler has to assume
  // could have changed any member: the entry store and the control store below
  // would otherwise force a reload of the pointer, and of the pool's base, on
  // the way to the next slot.
  static u8* slot(u8* region, u32 index) {
    return region + static_cast<usize>(index) * SLOT_BYTES;
  }

  static std::atomic_ref<u8> control(u8* region, u32 index) {
    return std::atomic_ref<u8>(*slot(region, index));
  }

  // Read and written through memcpy because a slot is five bytes long and the
  // entry inside it is not aligned for a StringPoolId. Each is a single load or
  // store, which is what the compiler turns them into.
  static StringPoolId entry(u8* region, u32 index) {
    StringPoolId id;
    std::memcpy(&id, slot(region, index) + 1, sizeof(id));
    return id;
  }

  static void write_entry(u8* region, u32 index, StringPoolId id) {
    std::memcpy(slot(region, index) + 1, &id, sizeof(id));
  }

  // Walks the probe sequence for @p hash once and reports what is at the end of
  // it.
  //
  // Linear probing stops at the first free slot, so a name can only be further
  // along the sequence than a free slot if it was never interned. That is what
  // makes one walk enough: the first free slot is the slot to intern into.
  //
  // A slot a writer has claimed is the one state a walk cannot resolve, because
  // the name this caller wants may be the one being published into it. The walk
  // goes past it either way and says so, and only the caller that means to
  // claim the free slot has to act on it.
  FPAG_ALWAYS_INLINE Probe lookup(u8* region,
                                  u64 hash,
                                  u8 fingerprint,
                                  std::string_view content) const {
    const u32 mask = static_cast<u32>(slots_ - 1);
    // The pool's base, read once for the same reason the region is: it is a
    // member of a member, so it is two dependent loads away from every name a
    // walk compares against.
    const char* const base = pool_->base();
    bool claimed = false;
    u32 index = static_cast<u32>(hash) & mask;

    for (u32 step = 0; step <= mask; ++step) {
      const u8 state = control(region, index).load(std::memory_order_acquire);
      if (state == fingerprint) [[likely]] {
        // A matching fingerprint is a candidate and not an answer: one bit in
        // seven is not a hash of the name, and a name that lost the race to be
        // interned first sits at an offset of its own. The length in front of
        // the name rejects what the fingerprint let through.
        const StringPoolId id = entry(region, index);
        const char* const data = base + id.offset;
        if (StringPool::length_at(data) == content.size() &&
            same_name(data, content.data(), content.size())) {
          return {id, index, true, false};
        }
      } else if (state == EMPTY_CONTROL) {
        return {{}, index, false, claimed};
      } else if (state == CLAIMED_CONTROL) {
        claimed = true;
      }
      index = (index + 1) & mask;
    }

    // Every slot was walked without meeting this name or an empty one: the
    // table is full. What that means is the caller's to say, so it is reported
    // rather than assumed.
    return {{}, index, false, claimed, true};
  }

  // Takes a free slot for one writer. False means another writer took it, and
  // the caller has to look again.
  //
  // Relaxed is enough: what a reader needs is for the entry and the name behind
  // it to be in place before the control byte is published, and that ordering
  // is the release store that follows. The claim itself only has to be atomic.
  static bool claim(u8* region, u32 index) {
    u8 expected = EMPTY_CONTROL;
    return control(region, index)
        .compare_exchange_strong(expected, CLAIMED_CONTROL,
                                 std::memory_order_relaxed,
                                 std::memory_order_relaxed);
  }

  void release() {
    if (region_ != nullptr) {
      mem::free_pages(region_, region_bytes_);
    }
    region_ = nullptr;
    slots_ = 0;
    region_bytes_ = 0;
    count_.store(0, std::memory_order_relaxed);
  }

  StringPool* pool_;
  u8* region_ = nullptr;
  usize slots_ = 0;
  usize region_bytes_ = 0;
  // On a line of its own: the count is written once per intern and read never
  // on the hot path, while the members above it are read on every probe, and a
  // reader sharing a line with a writer's increment takes the line's coherence
  // misses with it.
  alignas(mem::CACHE_LINE_SIZE) std::atomic<usize> count_{0};
  Hash hash_{};
};

}  // namespace str
