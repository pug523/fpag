// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace str {

struct StringPoolId {
  usize offset;
  usize length;
};

constexpr usize kInvalidOffset = 0xFFFFFFFFFFFFFFFF;
constexpr StringPoolId kInvalidStringPoolId = {kInvalidOffset, 0};
constexpr StringPoolId kEmptyStringId = {0, 0};

}  // namespace str
