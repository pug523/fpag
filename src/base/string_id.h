// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "base/mem/string_pool_id.h"

namespace base {

// struct StringId {
//   u32 id = kInvalidId;
//
//   constexpr StringId() = default;
//   // NOLINTNEXTLINE(google-explicit-constructor, runtime/explicit)
//   constexpr StringId(u32 i) : id(i) {}
//
//   static constexpr u32 kInvalidId = static_cast<u32>(-1);
//   inline constexpr bool is_valid() const { return id != kInvalidId; }
// };

using StringId = StringPoolId;

constexpr StringId kInvalidStringId = {0, 0, 0};

}  // namespace base
