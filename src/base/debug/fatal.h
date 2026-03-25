// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "base/debug/check.h"

namespace base::internal {

[[noreturn, gnu::cold]] void fatal_crash_impl();

[[noreturn, gnu::cold]] void unreachable_impl(const char* file,
                                              i32 line,
                                              const char* func,
                                              std::string_view msg = "");

}  // namespace base::internal

#define unreachable() \
  ::base::internal::unreachable_impl(__FILE__, __LINE__, __func__);

#define unreachable_msg(msg) \
  ::base::internal::unreachable_impl(__FILE__, __LINE__, __func__, msg);
