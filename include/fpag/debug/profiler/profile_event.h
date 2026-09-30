// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"
#include "fpag/debug/location.h"
#include "fpag/str/string_pool_id.h"

namespace debug {

// One measured span. The name and the category are ids into the Profiler that
// recorded the event rather than the text itself, which is what lets the macro
// take a std::string the caller built for it: the bytes are copied into the
// profiler's pool at construction, where the argument is still alive, and an
// event is then eight bytes smaller than the two pointers it replaces.
struct ProfileEvent {
  str::StringPoolId name = str::INVALID_STRING_POOL_ID;
  str::StringPoolId category = str::INVALID_STRING_POOL_ID;
  Location location = {};
  u64 start_time_ns = 0;
  u64 duration_ns = 0;
  u64 thread_id = 0;
  u32 process_id = 0;
};

}  // namespace debug
