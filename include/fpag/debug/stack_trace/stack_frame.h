// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"
#include "fpag/debug/location.h"

namespace debug {

struct StackTraceFrame {
  void* address = nullptr;
  usize index = 0;
  Location location = {};
};

}  // namespace debug
