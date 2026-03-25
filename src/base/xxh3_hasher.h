// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <string>
#include <string_view>

#include "base/numeric.h"

// NOLINTNEXTLINE(build/include_subdir)
#include "xxh3.h"

namespace base {

// A wrapper around the XXH3_64bits from xxhash.
struct Xxh3Hasher64 {
  inline u64 operator()(const std::string& str) const {
    return XXH3_64bits(str.data(), str.size());
  }

  inline u64 operator()(const std::string_view str) const {
    return XXH3_64bits(str.data(), str.size());
  }

  inline u64 operator()(const char* str) const {
    return XXH3_64bits(str, std::strlen(str));
  }
};

}  // namespace base
