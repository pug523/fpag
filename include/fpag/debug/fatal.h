// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/attributes.h"
#include "fpag/base/numeric.h"

namespace debug::internal {

[[noreturn]] FPAG_COLD void fatal_crash_impl();

// Records that a fatal path has written a resolved stack trace, so the
// signal handler that follows the trap can skip its raw one: the
// addresses would only repeat the frames with less in them.
void mark_stack_trace_printed();

// Whether a resolved trace has been written. The answer is a plain flag,
// so a signal handler may ask without allocating or locking.
[[nodiscard]] bool stack_trace_printed();

[[noreturn]] FPAG_COLD void unreachable_impl(const char* file,
                                             i32 line,
                                             const char* func,
                                             std::string_view msg = "");

}  // namespace debug::internal

#define FPAG_UNREACHABLE() \
  ::debug::internal::unreachable_impl(__FILE__, __LINE__, __func__);

#define FPAG_UNREACHABLE_MSG(msg) \
  ::debug::internal::unreachable_impl(__FILE__, __LINE__, __func__, msg);
