// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/debug/location.h"
#include "fpag/debug/profiler/profile_section.h"
#include "fpag/debug/profiler/profiler.h"

namespace debug {

class ProfileScope {
 public:
  ProfileScope(Profiler* profiler,
               const char* name,
               const Location& location,
               const char* category = "default") noexcept
      : section_(profiler, name, location, category) {
    section_.start();
  }

  ~ProfileScope() noexcept { section_.stop(); }

  void stop() noexcept { section_.stop(); }

  ProfileScope(const ProfileScope&) = delete;
  ProfileScope& operator=(const ProfileScope&) = delete;
  ProfileScope(ProfileScope&&) = delete;
  ProfileScope& operator=(ProfileScope&&) = delete;

 private:
  ProfileSection section_;
};

}  // namespace debug

#define FPAG_PROFILE_SCOPE_CONCAT_IMPL(a, b) a##b
#define FPAG_PROFILE_SCOPE_CONCAT(a, b) FPAG_PROFILE_SCOPE_CONCAT_IMPL(a, b)

// Profiler injection macros
#define PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(profiler, name, category)     \
  const ::debug::ProfileScope FPAG_PROFILE_SCOPE_CONCAT(                       \
      _profile_scope_, __LINE__)(profiler, name, ::debug::Location::current(), \
                                 category)

#define PROFILE_SCOPE_WITH_PROFILER(profiler, name) \
  PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(profiler, name, "default")

#define PROFILE_FUNCTION_WITH_PROFILER(profiler) \
  PROFILE_SCOPE_WITH_PROFILER(profiler, ::debug::Location::current().function)

// Default global profiler macros
#define PROFILE_SCOPE_WITH_CATEGORY(name, category)                            \
  PROFILE_SCOPE_WITH_CATEGORY_AND_PROFILER(&::debug::Profiler::global(), name, \
                                           category)

#define PROFILE_SCOPE(name) \
  PROFILE_SCOPE_WITH_PROFILER(&::debug::Profiler::global(), name)

#define PROFILE_FUNCTION() \
  PROFILE_FUNCTION_WITH_PROFILER(&::debug::Profiler::global())
