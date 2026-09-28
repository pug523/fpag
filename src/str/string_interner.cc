// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/str/string_interner.h"

#include <string_view>

namespace str {

StringInterner::StringId StringInterner::intern(const std::string_view str) {
  return table_.intern(str);
}

}  // namespace str
