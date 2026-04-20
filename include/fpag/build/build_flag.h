// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#define FPAG_BUILD_FLAG_CAT_INDIRECT(a, b) a##b
#define FPAG_BUILD_FLAG_CAT(a, b) FPAG_BUILD_FLAG_CAT_INDIRECT(a, b)

#define FPAG_BUILD_FLAG(flag) \
  (FPAG_BUILD_FLAG_CAT(FPAG_BUILD_FLAG_INTERNAL_, flag)())
