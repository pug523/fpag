// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "base/numeric.h"
#include "build/build_config.h"

namespace base {

// Captures the return addresses of the current call stack.
//
// This function performs a stack crawl starting from the caller's location.
// The resulting instruction pointers are stored in `out_frames`.
//
// Parameters:
//   out_frames - A pointer to an array of `void*` where the captured
//   addresses
//                will be stored. Must be at least `max_depth` in size.
//   max_depth  - The maximum number of frames to capture.
//   skip       - The number of additional frames to omit from the top of the
//                stack, starting from the caller of this function.
//
// Returns:
//   The number of addresses successfully written to `out_frames`.
//
// Note:
//   The accuracy of the backtrace depends on the platform and whether
//   frame pointers or unwind tables are available. This function is
//   designed to be safe to call during debugging or crash reporting.
usize capture_stack_addresses(void** out_frames,
                              usize max_depth,
                              usize skip = 0);

}  // namespace base
