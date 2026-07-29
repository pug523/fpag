// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <span>
#include <string_view>

#include "fpag/debug/profiler/profile_event.h"

namespace debug {

class TimeTraceFormatter {
 public:
  TimeTraceFormatter() = delete;

  // Formats and writes the given events as Time Trace JSON format
  // to the specified file path. Returns true on success.
  static bool write_to_file(std::string_view file_path,
                            std::span<const ProfileEvent> events);
};

}  // namespace debug
