// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/profiler/profiler.h"

#include <atomic>
#include <mutex>
#include <vector>

#include "fpag/base/numeric.h"
#include "fpag/debug/profiler/profile_event.h"

namespace debug {

Profiler::Profiler(usize initial_capacity) noexcept {
  events_.reserve(initial_capacity);
}

bool Profiler::is_enabled() const noexcept {
  return enabled_.load(std::memory_order_relaxed);
}

void Profiler::record_event(const ProfileEvent& event) noexcept {
  if (!is_enabled()) [[unlikely]] {
    return;
  }
  const std::lock_guard<std::mutex> lock(mutex_);
  events_.push_back(event);
}

void Profiler::start() noexcept {
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
  }
  enabled_.store(true, std::memory_order_release);
}

void Profiler::stop() noexcept {
  enabled_.store(false, std::memory_order_release);
}

void Profiler::clear() noexcept {
  const std::lock_guard<std::mutex> lock(mutex_);
  events_.clear();
}

std::vector<ProfileEvent> Profiler::copy_events() const noexcept {
  const std::lock_guard<std::mutex> lock(mutex_);
  return events_;
}

}  // namespace debug

