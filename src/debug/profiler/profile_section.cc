// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/profiler/profile_section.h"

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/debug/location.h"
#include "fpag/debug/process_id.h"
#include "fpag/debug/profiler/profile_event.h"
#include "fpag/debug/profiler/profiler.h"
#include "fpag/debug/thread_id.h"
#include "fpag/debug/time_util.h"

namespace debug {

ProfileSection::ProfileSection(Profiler* profiler,
                               const std::string_view name,
                               const Location& location,
                               const std::string_view category) noexcept
    : profiler_(profiler), location_(location) {
  if (profiler_ == nullptr) [[unlikely]] {
    return;
  }
  name_ = profiler_->intern(name);
  category_ = profiler_->intern(category);
}

void ProfileSection::start() noexcept {
  if (profiler_ != nullptr && profiler_->is_enabled()) [[likely]] {
    start_time_ns_ = debug::current_timestamp_ns();
    is_running_ = true;
  }
}

void ProfileSection::stop() noexcept {
  if (!is_running_) [[unlikely]] {
    return;
  }

  const u64 end_time_ns = debug::current_timestamp_ns();
  profiler_->record_event(ProfileEvent{
      .name = name_,
      .category = category_,
      .location = location_,
      .start_time_ns = start_time_ns_,
      .duration_ns = end_time_ns - start_time_ns_,
      .thread_id = current_thread_id(),
      .process_id = current_process_id(),
  });
  is_running_ = false;
}

}  // namespace debug
