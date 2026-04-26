// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace base {

enum class ColorMode : u8 {
  Off = 0,
  Ansi16 = 1,
  Ansi256 = 2,
  AnsiTrueColor = 3,
};

}  // namespace base
