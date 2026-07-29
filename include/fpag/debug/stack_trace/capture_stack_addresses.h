// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace base {

// Captures the return addresses of the current call stack.
// This function performs a stack crawl starting from the caller's location.
// The resulting instruction pointers are stored in `out_frames`.
usize capture_stack_addresses(void** out_frames,
                              usize max_depth,
                              usize skip = 0);

}  // namespace base
