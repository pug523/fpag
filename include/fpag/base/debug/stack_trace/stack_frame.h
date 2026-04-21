// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"

namespace base {

struct StackTraceFrame {
  void* address = nullptr;
  usize index = 0;
  std::string_view file = "";
  std::string_view function = "";
  u32 line = 0;
  u32 column = 0;
};

}  // namespace base
