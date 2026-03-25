// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "base/numeric.h"

namespace base {

struct StringPoolId {
  u32 block_id;
  usize offset;
  usize length;

  inline constexpr bool operator==(const StringPoolId& other) const {
    return block_id == other.block_id && offset == other.offset &&
           length == other.length;
  }
};

constexpr StringPoolId kInvalidStringPoolId = {0, 0, 0};

}  // namespace base
