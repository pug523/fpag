// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/debug/process_id.h"
#include "fpag/base/debug/thread_id.h"
#include "fpag/base/location.h"
#include "fpag/base/numeric.h"
#include "fpag/base/profiler/profile_event.h"
#include "fpag/base/profiler/profiler.h"
#include "fpag/base/time_util.h"

namespace base {

class ProfileScope {
 public:
  ProfileScope(Profiler* profiler,
               const char* name,
               const Location& location,
               const char* category = "default") noexcept
      : profiler_(profiler),
        name_(name),
        category_(category),
        location_(location) {
    if (profiler_ != nullptr && profiler_->is_enabled()) [[likely]] {
      start_time_ns_ = current_timestamp_ns();
    }
  }

  ~ProfileScope() noexcept {
    if (profiler_ == nullptr || !profiler_->is_enabled()) [[unlikely]] {
      return;
    }
    const u64 end_time_ns = current_timestamp_ns();
    profiler_->record_event(ProfileEvent{
        .name = name_,
        .category = category_,
        .location = location_,
        .start_time_ns = start_time_ns_,
        .duration_ns = end_time_ns - start_time_ns_,
        .thread_id = current_thread_id(),
        .process_id = current_process_id(),
    });
  }

  ProfileScope(const ProfileScope&) = delete;
  ProfileScope& operator=(const ProfileScope&) = delete;

  ProfileScope(ProfileScope&&) = delete;
  ProfileScope& operator=(ProfileScope&&) = delete;

 private:
  Profiler* profiler_ = nullptr;
  const char* name_ = nullptr;
  const char* category_ = nullptr;
  Location location_;
  u64 start_time_ns_ = 0;
};

}  // namespace base

#define FPAG_PROFILE_SCOPE_CONCAT_IMPL(a, b) a##b
#define FPAG_PROFILE_SCOPE_CONCAT(a, b) FPAG_PROFILE_SCOPE_CONCAT_IMPL(a, b)

// Profiler injection macros
#define PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(profiler, name, category)    \
  const ::base::ProfileScope FPAG_PROFILE_SCOPE_CONCAT(                       \
      _profile_scope_, __LINE__)(profiler, name, ::base::Location::current(), \
                                 category)

#define PROFILE_SCOPE_WITH_PROFILER(profiler, name) \
  PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(profiler, name, "default")

#define PROFILE_FUNCTION_WITH_PROFILER(profiler) \
  PROFILE_SCOPE_WITH_PROFILER(profiler, ::base::Location::current().function)

// Default global profiler macros
#define PROFILE_SCOPE_WITH_CATEGORY(name, category)                           \
  PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(&::base::Profiler::global(), name, \
                                           category)

#define PROFILE_SCOPE(name) \
  PROFILE_SCOPE_WITH_PROFILER(&::base::Profiler::global(), name)

#define PROFILE_FUNCTION() \
  PROFILE_FUNCTION_WITH_PROFILER(&::base::Profiler::global())
