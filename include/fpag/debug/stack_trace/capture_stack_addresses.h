// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace debug {

// Captures the return addresses of the current call stack.
// This function performs a stack crawl starting from the caller's location.
// The resulting instruction pointers are stored in `out_frames`.
usize capture_stack_addresses(void** out_frames,
                              usize max_depth,
                              usize skip = 0);

// The same, for a signal handler: no allocation, no lock, and no unwinder
// resolved on the way. The general form cannot promise that, because glibc's
// backtrace() opens libgcc on first use, so a handler that interrupted the
// loader would fault again inside itself. libgcc's unwinder and libunwind's
// local cursor are already loaded and need neither.
usize capture_stack_addresses_signal_safe(void** out_frames,
                                          usize max_depth,
                                          usize skip = 0);

}  // namespace debug
