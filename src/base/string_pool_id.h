// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "base/numeric.h"

namespace base {

struct StringPoolId {
  u32 chunk_id;
  usize offset;
  usize length;

  constexpr bool operator==(const StringPoolId& other) const {
    return chunk_id == other.chunk_id && offset == other.offset &&
           length == other.length;
  }
};

}  // namespace base
