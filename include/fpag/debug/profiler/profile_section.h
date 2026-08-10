// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"
#include "fpag/debug/location.h"
#include "fpag/debug/profiler/profiler.h"

namespace debug {

class ProfileSection {
 public:
  ProfileSection(Profiler* profiler,
                 const char* name,
                 const Location& location,
                 const char* category = "default") noexcept
      : profiler_(profiler),
        name_(name),
        category_(category),
        location_(location) {}
  ~ProfileSection() = default;

  ProfileSection(const ProfileSection&) = delete;
  ProfileSection& operator=(const ProfileSection&) = delete;

  ProfileSection(ProfileSection&&) = delete;
  ProfileSection& operator=(ProfileSection&&) = delete;

  void start() noexcept;
  void stop() noexcept;
  [[nodiscard]] bool is_running() const noexcept { return is_running_; }

 private:
  Profiler* profiler_ = nullptr;
  const char* name_ = nullptr;
  const char* category_ = nullptr;
  Location location_;
  u64 start_time_ns_ = 0;
  bool is_running_ = false;
};

#define PROFILE_SECTION_START_WITH_CATEGORY_AND_PROFILER(var_name, profiler, \
                                                         name, category)     \
  ::debug::ProfileSection var_name(profiler, name,                           \
                                   ::debug::Location::current(), category);  \
  var_name.start()

#define PROFILE_SECTION_START_WITH_PROFILER(var_name, profiler, name)        \
  PROFILE_SECTION_START_WITH_CATEGORY_AND_PROFILER(var_name, profiler, name, \
                                                   "default")

#define PROFILE_SECTION_END(var_name) var_name.stop()

#define PROFILE_SECTION_START_WITH_CATEGORY(var_name, name, category) \
  PROFILE_SECTION_START_WITH_CATEGORY_AND_PROFILER(                   \
      var_name, &::debug::Profiler::global(), name, category)

#define PROFILE_SECTION_START(var_name, name)                                 \
  PROFILE_SECTION_START_WITH_PROFILER(var_name, &::debug::Profiler::global(), \
                                      name)

}  // namespace debug
