// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <span>
#include <utility>

#include "fpag/base/numeric.h"
#include "fpag/debug/profiler/profile_event.h"

namespace debug {

class Profiler {
 public:
  static constexpr usize kDefaultMaxEvents =
      static_cast<usize>(1024 * 1024);  // 1M events pre-allocated

  explicit Profiler(usize max_events = kDefaultMaxEvents) noexcept
      : max_events_(max_events),
        events_(std::make_unique<ProfileEvent[]>(max_events)) {}

  ~Profiler() noexcept = default;

  Profiler(const Profiler&) = delete;
  Profiler& operator=(const Profiler&) = delete;

  Profiler(Profiler&& other) noexcept { construct_from(std::move(other)); }

  Profiler& operator=(Profiler&& other) noexcept {
    construct_from(std::move(other));
    return *this;
  }

  static Profiler& global() noexcept {
    static Profiler global_profiler;
    return global_profiler;
  }

  [[nodiscard]] bool is_enabled() const noexcept {
    return enabled_.load(std::memory_order_relaxed);
  }
  void record_event(const ProfileEvent& event) noexcept {
    if (!is_enabled()) [[unlikely]] {
      return;
    }
    const usize index = event_count_.fetch_add(1, std::memory_order_relaxed);
    if (index < max_events_) [[likely]] {
      events_[index] = event;
    }
  }

  void start() noexcept {
    event_count_.store(0, std::memory_order_relaxed);
    enabled_.store(true, std::memory_order_release);
  }

  void stop() noexcept { enabled_.store(false, std::memory_order_release); }

  [[nodiscard]] std::span<const ProfileEvent> events() const noexcept {
    const usize count =
        std::min(event_count_.load(std::memory_order_relaxed), max_events_);
    return std::span<const ProfileEvent>(events_.get(), count);
  }

 private:
  void construct_from(Profiler&& other) noexcept {
    max_events_ = other.max_events_;
    enabled_ = other.enabled_.load(std::memory_order_relaxed);
    event_count_ = other.event_count_.load(std::memory_order_relaxed);
    events_ = std::move(other.events_);

    other.max_events_ = 0;
    other.enabled_.store(false, std::memory_order_relaxed);
    other.event_count_.store(0, std::memory_order_relaxed);
    other.events_ = nullptr;
  }

  usize max_events_ = 0;
  std::atomic<bool> enabled_{false};
  std::atomic<usize> event_count_{0};
  std::unique_ptr<ProfileEvent[]> events_;
};

}  // namespace debug
