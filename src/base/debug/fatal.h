// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "base/debug/check.h"

// TODO: add fatal crash

#define unreachable()                                                    \
  do {                                                                   \
    ::base::internal::check_fail_impl("UNREACHABLE", __FILE__, __LINE__, \
                                      __func__);                         \
    __builtin_unreachable();                                             \
  } while (false)
