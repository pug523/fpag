// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/math_util.h"

#include <limits>
#include <type_traits>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"

namespace base {

TEST_CASE("is_power_of_two validates numbers correctly", "[base][math]") {
  SECTION("Positive powers of two") {
    CHECK(is_power_of_two(1));
    CHECK(is_power_of_two(2));
    CHECK(is_power_of_two(4));
    CHECK(is_power_of_two(1024));
    CHECK(is_power_of_two(1ull << 63));
  }

  SECTION("Non-powers of two") {
    CHECK_FALSE(is_power_of_two(0));
    CHECK_FALSE(is_power_of_two(3));
    CHECK_FALSE(is_power_of_two(7));
    CHECK_FALSE(is_power_of_two(1023));
    CHECK_FALSE(is_power_of_two(std::numeric_limits<u64>::max()));
  }

  SECTION("Negative numbers (if signed)") {
    if constexpr (std::is_signed_v<int>) {
      CHECK_FALSE(is_power_of_two(-2));
      CHECK_FALSE(is_power_of_two(-8));
    }
  }
}

TEST_CASE("next_power_of_two computes correct values", "[base][math]") {
  SECTION("Already a power of two") {
    // Should return the same value
    CHECK(next_power_of_two(1) == 1);
    CHECK(next_power_of_two(2) == 2);
    CHECK(next_power_of_two(8) == 8);
    CHECK(next_power_of_two(1024) == 1024);
  }

  SECTION("Between powers of two") {
    CHECK(next_power_of_two(3) == 4);
    CHECK(next_power_of_two(5) == 8);
    CHECK(next_power_of_two(7) == 8);
    CHECK(next_power_of_two(9) == 16);
    CHECK(next_power_of_two(1025) == 2048);
  }

  SECTION("Smallest input") {
    // 0 becomes 0 after --v, but bitwise ORs and ++v will result in 1.
    // However, it's generally used for v > 0.
    CHECK(next_power_of_two(0) == 1);
  }
}

TEST_CASE("next_power_of_two constexpr evaluation", "[base][math]") {
  // Ensure functions can be used in constant expressions
  constexpr bool is_p2 = base::is_power_of_two(16);
  constexpr u64 next_p2 = base::next_power_of_two(31);

  STATIC_CHECK(is_p2);
  STATIC_CHECK(next_p2 == 32);
}

}  // namespace base
