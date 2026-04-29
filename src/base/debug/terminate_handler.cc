// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/terminate_handler.h"

#include <exception>

#include "fpag/base/debug/fatal.h"
#include "fpag/base/debug/logger.h"
#include "fpag/base/debug/stack_trace/stack_trace.h"

namespace base {

namespace {

void terminate_handler() {
  debug_logger().fatal("Program terminated unexpectedly");
  print_stack_trace_from_here();
  internal::fatal_crash_impl();
}

}  // namespace

void register_terminate_handler() {
  std::set_terminate(terminate_handler);
}

}  // namespace base
