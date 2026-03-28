// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/stack_trace/stack_trace.h"

#include <string>
#include <vector>

#include "base/debug/stack_trace/stack_frame.h"
#include "base/numeric.h"
#include "build/attributes.h"
#include "build/build_config.h"
#include "catch2/catch_test_macros.hpp"

#if FPAG_BUILD_FLAG(IS_DEBUG)
#include "catch2/matchers/catch_matchers_string.hpp"
#endif

namespace base {

FPAG_NOINLINE void anonymous_func_inner(StackTrace* trace) {
  trace->collect_trace();
}

FPAG_NOINLINE void anonymous_func_outer(StackTrace* trace) {
  anonymous_func_inner(trace);
}

TEST_CASE("StackTrace Lifecycle and Boundary Tests", "[base][stack_trace]") {
  StackTrace trace;

  SECTION("Initial state is empty") {
    REQUIRE(trace.frame_count() == 0);
    REQUIRE(trace.frames() == nullptr);
  }

  SECTION("Initialization with various depths") {
    std::vector<StackTraceFrame> buffer(StackTrace::kMaxTraceDepth + 1);

    SECTION("Normal initialization") {
      trace.init(buffer.data(), 10);
      REQUIRE(trace.frame_count() == 0);
      REQUIRE(trace.frames() == buffer.data());
    }

    SECTION("Max depth initialization") {
      trace.init(buffer.data(), StackTrace::kMaxTraceDepth);
      REQUIRE(trace.frames() == buffer.data());
    }

    SECTION("Zero depth / skip initialization (edge case)") {
      trace.init(buffer.data(), 0, 0);
      trace.collect_trace();
      REQUIRE(trace.frame_count() == 0);
    }
  }
}

TEST_CASE("StackTrace Collection and Strings", "[base][stack_trace]") {
  StackTrace trace;
  const usize test_depth = 32;
  std::vector<StackTraceFrame> buffer(test_depth);

  trace.init(buffer.data(), test_depth);

  SECTION("Collect trace captures current stack") {
    trace.collect_trace();

    REQUIRE(trace.frame_count() > 0);
    REQUIRE(trace.frame_count() <= test_depth);

    REQUIRE(trace.frames() == buffer.data());

    SECTION("String representation is not empty") {
      std::string res = trace.to_string();
      REQUIRE_FALSE(res.empty());
    }
  }
}

TEST_CASE("StackTrace Edge Cases and Robustness", "[base][stack_trace]") {
  SECTION("Handling of very deep stacks") {
    StackTrace trace;
    const usize small_depth = 5;
    std::vector<StackTraceFrame> buffer(small_depth);

    trace.init(buffer.data(), small_depth);
    trace.collect_trace();

    REQUIRE(trace.frame_count() <= small_depth);
  }

  SECTION("Print with prefix") {
    StackTrace trace;
    std::vector<StackTraceFrame> buffer(10);
    trace.init(buffer.data(), 10);
    trace.collect_trace();
    // trace.print_trace("[DEBUG] stack trace test: ");
  }

  SECTION("String interning and stability") {
    StackTrace trace;
    std::vector<StackTraceFrame> buffer(10);
    trace.init(buffer.data(), 10);
    trace.collect_trace();

    std::string first_out = trace.to_string();
    std::string second_out = trace.to_string();

    REQUIRE(first_out == second_out);
  }
}

TEST_CASE("StackTrace Symbol Resolution", "[base][stack_trace]") {
  StackTrace trace;
  const usize test_depth = 64;
  std::vector<StackTraceFrame> buffer(test_depth);
  trace.init(buffer.data(), test_depth);

  SECTION("Captures functions in anonymous namespace") {
    anonymous_func_outer(&trace);

    REQUIRE(trace.frame_count() > 0);

    // trace.print_trace();

    // We only check the frame names in debug mode, as release builds do not
    // have debug information and inlines functions.
#if FPAG_BUILD_FLAG(IS_DEBUG)
    using Catch::Matchers::ContainsSubstring;
    const std::string trace_str = trace.to_string();
    REQUIRE_THAT(trace_str, ContainsSubstring("anonymous_func_inner"));
    REQUIRE_THAT(trace_str, ContainsSubstring("anonymous_func_outer"));
#endif
  }
}

}  // namespace base
