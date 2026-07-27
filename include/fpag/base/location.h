// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <source_location>
#include <string_view>

#include "fpag/base/numeric.h"

namespace base {

struct Location {
  const char* file = "";
  const char* function = "";
  u32 line = 0;
  u32 column = 0;

  constexpr std::string_view file_name() const { return file; }
  constexpr std::string_view function_name() const { return function; }

  constexpr bool valid_file() const { return file && file[0] != '\0'; }
  constexpr bool valid_function() const {
    return function && function[0] != '\0';
  }
  constexpr bool valid_line() const { return line > 0; }
  constexpr bool valid_column() const { return column > 0; }

  static constexpr Location current(
      std::source_location loc = std::source_location::current()) noexcept {
    return Location{
        .file = loc.file_name(),
        .function = loc.function_name(),
        .line = loc.line(),
        .column = loc.column(),
    };
  }
};

}  // namespace base

#define FROM_HERE() ::base::Location::current()
