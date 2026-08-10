// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <mutex>
#include <vector>

#include "fpag/base/numeric.h"
#include "fpag/debug/profiler/profile_event.h"

namespace debug {

class Profiler {
 public:
  static constexpr usize kDefaultInitialCapacity = static_cast<usize>(1024);

  explicit Profiler(usize initial_capacity = kDefaultInitialCapacity) noexcept;
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

 private:
  std::atomic<bool> enabled_{false};
  mutable std::mutex mutex_;
  std::vector<ProfileEvent> events_;
};

}  // namespace debug
