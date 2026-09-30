// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/debug/location.h"
#include "fpag/debug/profiler/profiler.h"
#include "fpag/str/string_pool_id.h"

namespace debug {

class ProfileSection {
 public:
  // The name and the category are interned here rather than at stop(), because
  // this is the only point where the caller's argument is guaranteed to be
  // alive: PROFILE_SCOPE(std::string(...)) dies at the end of the statement
  // that constructed this object, and stop() runs when the block closes.
  ProfileSection(Profiler* profiler,
                 std::string_view name,
                 const Location& location,
                 std::string_view category = "default") noexcept;
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
  str::StringPoolId name_ = str::INVALID_STRING_POOL_ID;
  str::StringPoolId category_ = str::INVALID_STRING_POOL_ID;
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
