// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace str {

// Fixed 8-byte identifier on all platforms (u32 offset + u32 length), so
// layouts embedding it (e.g. IR nodes) are architecture-independent.
// Offsets are valid only because StringPool capacity is capped well below
// 4 GiB (see StringPool).
struct StringPoolId {
  u32 offset;
  u32 length;
};

constexpr u32 INVALID_OFFSET = 0xFFFFFFFFu;
constexpr StringPoolId INVALID_STRING_POOL_ID = {INVALID_OFFSET, 0};
constexpr StringPoolId EMPTY_STRING_ID = {0, 0};

}  // namespace str
