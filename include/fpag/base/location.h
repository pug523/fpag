// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

namespace base {

struct Location {
  const char* file = "";
  const char* function = "";
  u32 line = 0;
  u32 column = 0;

  constexpr std::string_view file_name() const { return file; };
  constexpr std::string_view function_name() const { return function; };

  constexpr bool valid_file() const { return file[0] != '\0'; }
  constexpr bool valid_function() const { return function[0] != '\0'; }
  constexpr bool valid_line() const { return line > 0; }
  constexpr bool valid_column() const { return column > 0; }
};

}  // namespace base

#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
#define BASE_LOCATION_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#define BASE_LOCATION_PRETTY_FUNCTION __FUNCSIG__
#else
#define BASE_LOCATION_PRETTY_FUNCTION __func__
#endif

#if defined(__has_builtin) && __has_builtin(__builtin_COLUMN)
#define BASE_LOCATION_COLUMN __builtin_COLUMN()
#else
#define BASE_LOCATION_COLUMN 0
#endif

#define FROM_HERE()                                              \
  ::base::Location {                                             \
    .file = __FILE__, .function = BASE_LOCATION_PRETTY_FUNCTION, \
    .line = __LINE__, .column = BASE_LOCATION_COLUMN,            \
  }
