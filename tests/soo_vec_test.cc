// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/soo_vec.h"

#include <string>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/idx.h"
#include "fpag/base/numeric.h"
#include "fpag/base/soo_vec_slice.h"

namespace base {

namespace {

struct TestTag {};
using TestIdx = Idx<TestTag, u32>;

struct MoveOnlyType {
  i32 val = 0;
  explicit MoveOnlyType(i32 v) : val(v) {}
  ~MoveOnlyType() = default;

  MoveOnlyType(const MoveOnlyType&) = delete;
  MoveOnlyType& operator=(const MoveOnlyType&) = delete;

  MoveOnlyType(MoveOnlyType&&) noexcept = default;
  MoveOnlyType& operator=(MoveOnlyType&&) noexcept = default;
};

// Poisons itself on move and on destruction, so a value read after either
// carries the poison instead of whatever the storage happens to hold.
struct PoisonedValue {
  static constexpr i32 kPoison = -1;

  i32 val = 0;

  explicit PoisonedValue(i32 v) : val(v) {}

  PoisonedValue(const PoisonedValue&) = default;
  PoisonedValue& operator=(const PoisonedValue&) = default;

  PoisonedValue(PoisonedValue&& other) noexcept : val(other.val) {
    other.val = kPoison;
  }
  PoisonedValue& operator=(PoisonedValue&& other) noexcept = default;

  ~PoisonedValue() { val = kPoison; }
};

}  // namespace

TEST_CASE("SooVec inline vs dynamic storage", "[base][soo_vec]") {
  SECTION("Inline storage capacity limit (N=2)") {
    SooVec<i32, TestIdx, 2> vec;

    CHECK(vec.capacity() == 2);
    vec.push_back(10);
    vec.push_back(20);

    // Still in inline storage
    CHECK(vec.size() == 2);
    CHECK(vec.capacity() == 2);
    CHECK(vec[TestIdx{0}] == 10);
    CHECK(vec[TestIdx{1}] == 20);

    // Transition to dynamic allocation
    vec.push_back(30);
    CHECK(vec.size() == 3);
    CHECK(vec.capacity() > 2);
    CHECK(vec[TestIdx{0}] == 10);
    CHECK(vec[TestIdx{1}] == 20);
    CHECK(vec[TestIdx{2}] == 30);
  }

  SECTION("Move-only type support across reallocation") {
    SooVec<MoveOnlyType, TestIdx, 2> vec;

    vec.emplace_back(100);
    vec.emplace_back(200);

    // Trigger reallocation
    vec.emplace_back(300);

    CHECK(vec.size() == 3);
    CHECK(vec[TestIdx{0}].val == 100);
    CHECK(vec[TestIdx{1}].val == 200);
    CHECK(vec[TestIdx{2}].val == 300);
  }

  SECTION("Copy and Move semantics") {
    SooVec<std::string, TestIdx, 2> original;
    original.push_back("hello");
    original.push_back("world");

    SECTION("Copy construction") {
      SooVec<std::string, TestIdx, 2> copy = original;
      CHECK(copy.size() == 2);
      CHECK(copy[TestIdx{0}] == "hello");
      CHECK(original[TestIdx{0}] == "hello");  // Original unchanged
    }

    SECTION("Move construction") {
      SooVec<std::string, TestIdx, 2> moved = std::move(original);
      CHECK(moved.size() == 2);
      CHECK(moved[TestIdx{0}] == "hello");
      CHECK(original.empty());  // NOLINT(bugprone-use-after-move)
    }
  }

  SECTION("Initializer list constructor") {
    SooVec<i32, TestIdx, 4> vec{10, 20, 30};
    CHECK(vec.size() == 3);
    CHECK(vec.front() == 10);
    CHECK(vec.back() == 30);
  }

  SECTION("An argument that names an element across reallocation") {
    SooVec<PoisonedValue, TestIdx, 2> vec;
    vec.emplace_back(100);
    vec.emplace_back(200);
    vec.emplace_back(300);
    vec.emplace_back(400);

    REQUIRE(vec.capacity() == 4);
    REQUIRE(vec.size() == 4);

    // Growing moves every element into a new block and releases the block the
    // argument still names.
    vec.emplace_back(vec[TestIdx{0}]);

    CHECK(vec.size() == 5);
    CHECK(vec[TestIdx{0}].val == 100);
    CHECK(vec[TestIdx{3}].val == 400);
    CHECK(vec[TestIdx{4}].val == 100);
  }
}

TEST_CASE("SooVec without inline storage allocates for the first element",
          "[base][soo_vec]") {
  SooVec<i32, TestIdx, 0> vec;

  CHECK(vec.empty());
  CHECK(vec.capacity() == 0);

  const TestIdx first = vec.emplace_back(1);
  CHECK(vec.size() == 1);
  CHECK(vec.capacity() >= 1);
  CHECK(vec[first] == 1);

  vec.emplace_back(2);
  CHECK(vec.size() == 2);
  CHECK(vec[TestIdx{0}] == 1);
  CHECK(vec[TestIdx{1}] == 2);
}

TEST_CASE("SooVecSlice operations", "[base][soo_vec_slice]") {
  SooVec<i32, TestIdx, 4> vec{1, 2, 3, 4, 5};

  SECTION("Subslice and slicing helpers") {
    const SooVecSlice<i32, TestIdx> slice = vec.slice();
    CHECK(slice.size() == 5);

    // Non-explicit implicit conversion check
    const ConstSooVecSlice<i32, TestIdx> const_slice = slice;
    CHECK(const_slice.size() == 5);

    auto sub = slice.subslice(1, 3);
    CHECK(sub.size() == 3);
    CHECK(sub.front() == 2);
    CHECK(sub.back() == 4);
  }

  SECTION("Reverse iteration") {
    SooVec<i32, TestIdx, 4> v{10, 20, 30};
    auto slice = v.slice();

    i32 expected = 30;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = slice.rbegin(); it != slice.rend(); ++it) {
      CHECK(*it == expected);
      expected -= 10;
    }
  }

  SECTION("pop_front and pop_back") {
    SooVecSlice<i32, TestIdx> slice = vec.slice();

    slice.pop_front(2);
    CHECK(slice.size() == 3);
    CHECK(slice.front() == 3);

    slice.pop_back(1);
    CHECK(slice.size() == 2);
    CHECK(slice.back() == 4);
  }
}

}  // namespace base
