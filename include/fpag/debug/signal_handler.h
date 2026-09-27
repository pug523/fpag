// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace debug {

// Size of the stack the signal handlers run on, installed by
// register_signal_handlers(). A crash often arrives with the thread stack
// exhausted, which is the one moment a trace cannot be taken; the handlers get
// room of their own so that there is somewhere to take it from.
inline constexpr usize SIGNAL_STACK_BYTES = 64 * 1024;

void register_signal_handlers();

// True when the caller is running on the stack register_signal_handlers()
// installed, rather than on the thread stack. POSIX has no way to ask the
// kernel that, so the mapping is remembered here instead. A crash report says
// which of the two produced it: a handler that had nowhere of its own to run is
// the case where a report goes missing.
bool running_on_signal_stack() noexcept;

}  // namespace debug
