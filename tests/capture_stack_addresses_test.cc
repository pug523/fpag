// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/stack_trace/capture_stack_addresses.h"

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/build/build_flag.h"

#if FPAG_BUILD_FLAG(IS_DEBUG)
#include "fpag/base/attributes.h"
#endif

namespace debug {

TEST_CASE("capture_stack_addresses basic functionality",
          "[base][debug][stack_trace]") {
  const usize MAX_DEPTH = 10;
  void* frames[MAX_DEPTH];

  SECTION("Capture at least one frame") {
    // Basic capture to ensure the function returns a non-zero value in a
    // standard environment.
    const usize captured = capture_stack_addresses(frames, MAX_DEPTH, 0);

    CHECK(captured <= MAX_DEPTH);
#if FPAG_BUILD_FLAG(IS_OS_ASMJS)
    // The wasm build has no unwinder, so capture reports no frames by design.
    CHECK(captured == 0);
#else
    CHECK(captured > 0);

    // Ensure the addresses are not all null.
    bool found_valid_address = false;
    for (usize i = 0; i < captured; ++i) {
      if (frames[i] != nullptr) {
        found_valid_address = true;
        break;
      }
    }
    CHECK(found_valid_address);
#endif
  }

  SECTION("Respect max_depth limit") {
    const usize SMALL_DEPTH = 2;
    void* small_frames[SMALL_DEPTH];

    // Even if the stack is deep, it should only return up to SMALL_DEPTH.
    const usize captured =
        capture_stack_addresses(small_frames, SMALL_DEPTH, 0);

    CHECK(captured <= SMALL_DEPTH);
  }

  SECTION("Handle zero max_depth") {
    void* no_frames[1];
    const usize captured = capture_stack_addresses(no_frames, 0, 0);

    CHECK(captured == 0);
  }
}

// We only test skip functionality in debug builds because release builds may
// inline `deep_stack_function` even with `FPAG_NOINLINE`.
#if FPAG_BUILD_FLAG(IS_DEBUG)

FPAG_NOINLINE usize deep_stack_function(void** out_frames,
                                        usize max_depth,
                                        usize skip) {
  const usize result = capture_stack_addresses(out_frames, max_depth, skip);
  return result;
}

TEST_CASE("capture_stack_addresses skip functionality",
          "[base][debug][stack_trace]") {
  const usize MAX_DEPTH = 10;
  void* frames_normal[MAX_DEPTH];
  void* frames_skipped[MAX_DEPTH];

  SECTION("Skip shifts the captured addresses") {
    // Capture without extra skip.
    const usize count_normal = deep_stack_function(frames_normal, MAX_DEPTH, 0);

    // Capture skipping the DeepStackFunction itself.
    const usize count_skipped =
        deep_stack_function(frames_skipped, MAX_DEPTH, 1);

    if (count_normal > 1 && count_skipped > 0) {
      // The first frame of the skipped capture should match
      // the second frame of the normal capture.
      CHECK(frames_skipped[0] == frames_normal[1]);
    }
  }

  SECTION("Excessive skip returns zero or minimal frames") {
    // Skipping more frames than likely exist in this test runner context.
    const usize captured =
        capture_stack_addresses(frames_normal, MAX_DEPTH, 1000);

    // Depending on implementation, this usually returns 0 if the stack is
    // exhausted.
    CHECK(captured == 0);
  }
}

#endif

}  // namespace debug
