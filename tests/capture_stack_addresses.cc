// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/stack_trace/capture_stack_addresses.h"

#include <vector>

#include "base/numeric.h"
#include "build/attributes.h"
#include "catch2/catch_test_macros.hpp"

namespace base {

TEST_CASE("capture_stack_addresses basic functionality",
          "[base][debug][stack_trace]") {
  const usize kMaxDepth = 10;
  void* frames[kMaxDepth];

  SECTION("Capture at least one frame") {
    // Basic capture to ensure the function returns a non-zero value in a
    // standard environment.
    usize captured = capture_stack_addresses(frames, kMaxDepth, 0);

    CHECK(captured > 0);
    CHECK(captured <= kMaxDepth);

    // Ensure the addresses are not all null.
    bool found_valid_address = false;
    for (usize i = 0; i < captured; ++i) {
      if (frames[i] != nullptr) {
        found_valid_address = true;
        break;
      }
    }
    CHECK(found_valid_address);
  }

  SECTION("Respect max_depth limit") {
    const usize kSmallDepth = 2;
    void* small_frames[kSmallDepth];

    // Even if the stack is deep, it should only return up to kSmallDepth.
    usize captured = capture_stack_addresses(small_frames, kSmallDepth, 0);

    CHECK(captured <= kSmallDepth);
  }

  SECTION("Handle zero max_depth") {
    void* no_frames[1];
    usize captured = capture_stack_addresses(no_frames, 0, 0);

    CHECK(captured == 0);
  }
}

FPAG_NOINLINE usize deep_stack_function(void** out_frames,
                                        usize max_depth,
                                        usize skip) {
  return capture_stack_addresses(out_frames, max_depth, skip);
}

TEST_CASE("capture_stack_addresses skip functionality",
          "[base][debug][stack_trace]") {
  const usize kMaxDepth = 10;
  void* frames_normal[kMaxDepth];
  void* frames_skipped[kMaxDepth];

  SECTION("Skip shifts the captured addresses") {
    // Capture without extra skip.
    usize count_normal = deep_stack_function(frames_normal, kMaxDepth, 0);

    // Capture skipping the DeepStackFunction itself.
    usize count_skipped = deep_stack_function(frames_skipped, kMaxDepth, 1);

    if (count_normal > 1 && count_skipped > 0) {
      // The first frame of the skipped capture should match
      // the second frame of the normal capture.
      CHECK(frames_skipped[0] == frames_normal[1]);
    }
  }

  SECTION("Excessive skip returns zero or minimal frames") {
    // Skipping more frames than likely exist in this test runner context.
    usize captured = capture_stack_addresses(frames_normal, kMaxDepth, 1000);

    // Depending on implementation, this usually returns 0 if the stack is
    // exhausted.
    CHECK(captured == 0);
  }
}

}  // namespace base
