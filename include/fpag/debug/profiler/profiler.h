// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <mutex>
#include <string_view>
#include <vector>

#include "fpag/base/numeric.h"
#include "fpag/debug/profiler/profile_event.h"
#include "fpag/str/string_interner.h"

namespace debug {

class Profiler {
 public:
  static constexpr usize DEFAULT_INITIAL_CAPACITY = static_cast<usize>(1024);

  // How many distinct names and categories the interner is sized for. The
  // interner's own default is 16.7M names, which reserves 160 MiB of table
  // whose pages are four KiB resident each per name a probe scatters across it,
  // so a program that profiles a few thousand call sites spends 130 MB on the
  // table rather than 640 KiB. A profiler's names are call site labels, and
  // 64K of them is far more than a program has.
  static constexpr usize DEFAULT_INTERNER_NAMES = static_cast<usize>(64 * 1024);
  // Enough pool for 64K names averaging 128 bytes. Exceeding it is a check, not
  // a slow growth, so the headroom is deliberate.
  static constexpr usize DEFAULT_INTERNER_POOL_BYTES =
      static_cast<usize>(8 * 1024 * 1024);

  explicit Profiler(
      usize initial_capacity = DEFAULT_INITIAL_CAPACITY,
      usize interner_names = DEFAULT_INTERNER_NAMES,
      usize interner_pool_bytes = DEFAULT_INTERNER_POOL_BYTES) noexcept;
  ~Profiler() noexcept = default;

  Profiler(const Profiler&) = delete;
  Profiler& operator=(const Profiler&) = delete;

  Profiler(Profiler&&) = delete;
  Profiler& operator=(Profiler&&) = delete;

  static Profiler& global() noexcept {
    static Profiler global_profiler;
    return global_profiler;
  }

  [[nodiscard]] bool is_enabled() const noexcept;

  void record_event(const ProfileEvent& event) noexcept;

  void start() noexcept;
  void stop() noexcept;
  void clear() noexcept;

  [[nodiscard]] std::vector<ProfileEvent> copy_events() const noexcept;

  [[nodiscard]] usize size() const noexcept { return events_.size(); }
  [[nodiscard]] bool empty() const noexcept { return events_.empty(); }

  // The name an event's id names, which is the only way to read one: the event
  // holds an id, not text. An id this profiler never handed out is not a name.
  str::StringPoolId intern(std::string_view name) noexcept;
  [[nodiscard]] std::string_view name(str::StringPoolId id) const noexcept {
    return interner_.get(id);
  }

  // The interner the events' ids name into, for a formatter that writes them
  // out. It is the one that handed out those ids and the only one that can
  // resolve them.
  [[nodiscard]] const str::StringInterner& interner() const noexcept {
    return interner_;
  }

 private:
  std::atomic<bool> enabled_{false};
  mutable std::mutex mutex_;
  std::vector<ProfileEvent> events_;
  // Never reset, not by start() and not by clear(): an id a caller is still
  // holding has to keep naming the same bytes after the events are dropped.
  str::StringInterner interner_;
};

}  // namespace debug
