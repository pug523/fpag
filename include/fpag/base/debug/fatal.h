// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/build/attributes.h"

namespace base::internal {

[[noreturn]] FPAG_COLD void fatal_crash_impl();

[[noreturn]] FPAG_COLD void unreachable_impl(const char* file,
                                             i32 line,
                                             const char* func,
                                             std::string_view msg = "");

}  // namespace base::internal

#define FPAG_UNREACHABLE() \
  ::base::internal::unreachable_impl(__FILE__, __LINE__, __func__);

#define FPAG_UNREACHABLE_MSG(msg) \
  ::base::internal::unreachable_impl(__FILE__, __LINE__, __func__, msg);
