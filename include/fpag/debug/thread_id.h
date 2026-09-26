// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace debug {

// The current thread's identifier. The OS thread id on Windows, the pthread_t
// on POSIX, and the Mach thread port on Apple. Defined in the library rather
// than inline here so that this header does not pull in a platform SDK.
u64 current_thread_id() noexcept;

}  // namespace debug
