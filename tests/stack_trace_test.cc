// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/stack_trace/stack_trace.h"

#include <string>
#include <string_view>
#include <vector>

#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"
#include "fpag/base/attributes.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/debug/stack_trace/stack_frame.h"
#include "fpag/debug/stack_trace/symbolicator.h"

#if FPAG_BUILD_FLAG(IS_DEBUG)
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#endif

namespace debug {

FPAG_NOINLINE void anonymous_func_inner(StackTrace* trace) {
  trace->collect_trace();
}

FPAG_NOINLINE void anonymous_func_outer(StackTrace* trace) {
  anonymous_func_inner(trace);
}

// Nested so that the demangled name of the frame below is several times longer
// than a function name usually is. The trace used to reserve a fixed budget per
// frame and intern into a vector, so a few of these overshot the reservation
// and left the names interned before the reallocation dangling.
//
// External linkage on purpose: a name only reaches a trace when the symbol is
// exported, and an anonymous namespace function is never exported.
template <typename T>
struct Nested {
  using type = Nested<Nested<T>>;
};

using LongName = Nested<Nested<Nested<Nested<Nested<Nested<
    Nested<Nested<Nested<Nested<Nested<Nested<Nested<Nested<Nested<Nested<
        Nested<Nested<Nested<Nested<Nested<Nested<Nested<Nested<Nested<Nested<
            Nested<Nested<Nested<Nested<int>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>;

template <typename T>
FPAG_NOINLINE void descend(usize depth, StackTrace* trace) {
  if (depth == 0) {
    trace->collect_trace();
    return;
  }
  descend<T>(depth - 1, trace);
}

TEST_CASE("StackTrace Lifecycle and Boundary Tests", "[base][stack_trace]") {
  StackTrace trace;

  SECTION("Initial state is empty") {
    REQUIRE(trace.frame_count() == 0);
    REQUIRE(trace.frames() == nullptr);
  }

  SECTION("Initialization with various depths") {
    std::vector<StackTraceFrame> buffer(StackTrace::MAX_TRACE_DEPTH + 1);

    SECTION("Normal initialization") {
      trace.init(buffer.data(), 10);
      REQUIRE(trace.frame_count() == 0);
      REQUIRE(trace.frames() == buffer.data());
    }

    SECTION("Max depth initialization") {
      trace.init(buffer.data(), StackTrace::MAX_TRACE_DEPTH);
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

#if FPAG_BUILD_FLAG(IS_OS_ASMJS)
    // The wasm build has no unwinder, so a collected trace is empty by design.
    REQUIRE(trace.frame_count() == 0);
#else
    REQUIRE(trace.frame_count() > 0);
    REQUIRE(trace.frame_count() <= test_depth);

    REQUIRE(trace.frames() == buffer.data());

    SECTION("String representation is not empty") {
      const std::string res = trace.to_string();
      REQUIRE_FALSE(res.empty());
    }
#endif
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

#if FPAG_BUILD_FLAG(IS_OS_ASMJS)
    // The wasm build has no unwinder, so there are no frames to resolve.
    REQUIRE(trace.frame_count() == 0);
#else
    REQUIRE(trace.frame_count() > 0);

    // We only check the frame names in debug mode, as release builds do not
    // have debug information and inlines functions.
#if FPAG_BUILD_FLAG(IS_DEBUG)
    using Catch::Matchers::ContainsSubstring;
    const std::string trace_str = trace.to_string();
    REQUIRE_THAT(trace_str, ContainsSubstring("anonymous_func_inner"));
    REQUIRE_THAT(trace_str, ContainsSubstring("anonymous_func_outer"));
#endif
#endif
  }
}

// Names only reach a trace where the binary exports its symbols, and the
// project exports them for a debug build, so this case is a debug one. The
// collection itself is not: what it checks is where the names end up.
#if FPAG_BUILD_FLAG(IS_DEBUG)
TEST_CASE("StackTrace keeps every name when one is very long",
          "[base][stack_trace]") {
  StackTrace trace;
  // Deep enough that the long names outweigh any per-frame budget: the frames
  // the test runner itself contributes are short, so they cannot make up the
  // difference.
  const usize test_depth = 160;
  std::vector<StackTraceFrame> buffer(test_depth);
  trace.init(buffer.data(), test_depth, 2);

  descend<LongName>(120, &trace);

#if FPAG_BUILD_FLAG(IS_OS_ASMJS)
  // The wasm build has no unwinder, so there is nothing to keep alive.
  REQUIRE(trace.frame_count() == 0);
#else
  // The demangled name every frame of the descent is expected to start with.
  constexpr std::string_view DESCEND_PREFIX = "void debug::descend";

  REQUIRE(trace.frame_count() > 32);

  usize named = 0;
  const char* previous = nullptr;
  for (usize i = 0; i < trace.frame_count(); ++i) {
    const StackTraceFrame& frame = trace.frames()[i];
    const std::string_view name = frame.location.function_name();
    const std::string_view file = frame.location.file_name();
    if (!name.starts_with(DESCEND_PREFIX)) {
      continue;
    }
    ++named;

    // A frame's name and its file are interned one after the other, so the next
    // frame's name starts after both. A buffer that reallocated while the later
    // and longer names were being added leaves the earlier ones pointing into
    // freed storage, which still reads the right bytes often enough that
    // comparing names against to_string() would not notice; the layout always
    // does.
    INFO("frame " << i);
    if (previous != nullptr) {
      CHECK(name.data() == previous);
    }
    CHECK(file.data() == name.data() + name.size() + 1);
    previous = file.data() + file.size() + 1;
  }

  // Deep enough that the names outweigh any per-frame budget the storage could
  // have reserved: the frames the test runner contributes are short, so they
  // cannot make up the difference.
  CHECK(named > 32);
#endif
}
#endif

// The rest of this file is about what a single address resolves to, which on
// Linux and Android is read out of the object file itself. A release build
// carries no debug information to read, so the source position is a debug case;
// the function name is not, because it comes from the symbol table.
#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
namespace {

// Resolves the address the call above it returns to, which is inside the
// caller. A return address is the instruction after the call, so it is taken
// back by one to land on the call itself: that is what makes the line the
// calling line, and it is what a stack frame address needs as well.
FPAG_NOINLINE SymbolInfo resolve_caller() {
  const auto* const returns =
      static_cast<const u8*>(__builtin_return_address(0));
  return Symbolicator{}.resolve(returns - 1);
}

// Local linkage, so the dynamic symbol table has no name for it and the object
// file's own symbol table is the only place a name can come from. The line the
// call is written on comes back with the answer, so a test can check what the
// address resolves to without hard coding a number a later edit would move.
FPAG_NOINLINE SymbolInfo
resolve_caller_in_an_anonymous_namespace(u32* call_line) {
  const u32 line = __LINE__ + 1;
  const SymbolInfo info = resolve_caller();
  *call_line = line;
  return info;
}

}  // namespace

TEST_CASE("Symbolication names a function the dynamic table leaves out",
          "[base][stack_trace]") {
  u32 call_line = 0;
  const SymbolInfo info = resolve_caller_in_an_anonymous_namespace(&call_line);

  REQUIRE(info.resolved);
  REQUIRE(info.function.find("resolve_caller_in_an_anonymous_namespace") !=
          std::string::npos);
}

#if FPAG_BUILD_FLAG(IS_DEBUG)
TEST_CASE("Symbolication places an address at the line that called",
          "[base][stack_trace]") {
  constexpr std::string_view THIS_FILE = "stack_trace_test.cc";

  u32 call_line = 0;
  const SymbolInfo info = resolve_caller_in_an_anonymous_namespace(&call_line);

  REQUIRE(info.resolved);
  REQUIRE(std::string_view(info.file).ends_with(THIS_FILE));
  CHECK(info.line == call_line);
  CHECK(info.column > 0);
}
#endif

#endif

}  // namespace debug
