// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace str {

// The bytes a name is preceded by in a StringPool, holding its length. The id
// is the offset of the bytes and nothing else, so this is where the length
// lives: an id a caller embeds in a node stays four bytes wide, and a name
// costs the same to read from wherever it is reached.
constexpr usize LENGTH_PREFIX_BYTES = sizeof(u32);

// Fixed 4-byte identifier on all platforms (a u32 offset), so layouts embedding
// it (e.g. IR nodes) are architecture-independent. Offsets are valid only
// because StringPool capacity is capped well below 4 GiB (see StringPool).
struct StringPoolId {
  u32 offset;
};

// Two ids name the same name when their offsets are the same, which is what a
// caller comparing an id it holds against one it was just handed wants to say.
constexpr bool operator==(StringPoolId lhs, StringPoolId rhs) {
  return lhs.offset == rhs.offset;
}
constexpr bool operator!=(StringPoolId lhs, StringPoolId rhs) {
  return !(lhs == rhs);
}

constexpr u32 INVALID_OFFSET = 0xFFFFFFFFu;
constexpr StringPoolId INVALID_STRING_POOL_ID = {INVALID_OFFSET};
// The empty name has no bytes, so its id names the zero length the pool keeps
// at offset zero. get() reads an empty view out of it, which is what lets the
// pool treat the empty name as a name rather than as a special case.
constexpr StringPoolId EMPTY_STRING_ID = {
    static_cast<u32>(LENGTH_PREFIX_BYTES)};

}  // namespace str
