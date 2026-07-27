// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/location.h"
#include "fpag/base/numeric.h"

namespace base {

struct ProfileEvent {
  const char* name = nullptr;
  const char* category = "default";
  Location location = {};
  u64 start_time_ns = 0;
  u64 duration_ns = 0;
  u64 thread_id = 0;
  u32 process_id = 0;
};

}  // namespace base
