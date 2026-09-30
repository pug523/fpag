// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <span>
#include <string_view>

#include "fpag/debug/profiler/profile_event.h"
#include "fpag/str/string_interner.h"

namespace debug {

class TimeTraceFormatter {
 public:
  TimeTraceFormatter() = delete;

  // Formats and writes the given events as Time Trace JSON format
  // to the specified file path. Returns true on success.
  //
  // @p interner is the interner the events' names and categories were
  // interned into, which is what turns their ids back into text. An event
  // whose id is INVALID_STRING_POOL_ID formats as "unnamed" or "default": the
  // event holds an id rather than the name, so there is nothing else to print.
  static bool write_to_file(std::string_view file_path,
                            std::span<const ProfileEvent> events,
                            const str::StringInterner& interner);
};

}  // namespace debug
