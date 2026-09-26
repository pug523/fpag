// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace debug {

// The current process's identifier. Defined in the library rather than inline
// here so that this header does not pull in a platform SDK.
u32 current_process_id() noexcept;

}  // namespace debug
