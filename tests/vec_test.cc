// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/vec.h"

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/idx.h"
#include "fpag/base/numeric.h"
#include "fpag/base/vec_slice.h"

namespace base {

namespace {

struct TestTag {};
using TestIdx = Idx<TestTag, u32>;

}  // namespace

TEST_CASE("Vec basic operations", "[base][vec]") {
  Vec<i32, TestIdx> vec;

  SECTION("Initialization and capacity management") {
    CHECK(vec.empty());
    CHECK(vec.size() == 0);

    vec.reserve(10);
    CHECK(vec.capacity() >= 10);
    CHECK(vec.empty());

    vec.resize(5, 42);
    CHECK(vec.size() == 5);
    CHECK(vec[TestIdx{0}] == 42);
    CHECK(vec[TestIdx{4}] == 42);

    vec.clear();
    CHECK(vec.empty());
    CHECK(vec.size() == 0);
  }

  SECTION("Push, emplace and pop") {
    const TestIdx id0 = vec.emplace_back(10);
    const TestIdx id1 = vec.push_back(20);
    const i32 val = 30;
    const TestIdx id2 = vec.push_back(val);

    CHECK(id0.idx == 0);
    CHECK(id1.idx == 1);
    CHECK(id2.idx == 2);
    CHECK(vec.size() == 3);

    CHECK(vec[id0] == 10);
    CHECK(vec[id1] == 20);
    CHECK(vec[id2] == 30);
    CHECK(vec.front() == 10);
    CHECK(vec.back() == 30);

    vec.pop_back();
    CHECK(vec.size() == 2);
    CHECK(vec.back() == 20);
  }

  SECTION("Initializer list constructor") {
    const Vec<i32, TestIdx> init_vec{1, 2, 3, 4};
    CHECK(init_vec.size() == 4);
    CHECK(init_vec[TestIdx{0}] == 1);
    CHECK(init_vec[TestIdx{3}] == 4);
    CHECK(init_vec[usize{2}] == 3);
  }

  SECTION("Iterators") {
    Vec<i32, TestIdx> v{10, 20, 30};

    i32 sum = 0;
    for (const i32 val : v) {
      sum += val;
    }
    CHECK(sum == 60);

    auto it = v.begin();
    *it = 100;
    CHECK(v[TestIdx{0}] == 100);

    CHECK(v.cbegin() != v.cend());
    CHECK(v.crbegin() != v.crend());
    CHECK(*v.crbegin() == 30);
  }

  SECTION("IdxRange access") {
    Vec<i32, TestIdx> v{10, 20, 30};
    const auto range = v.idx_range();

    CHECK(range.size() == 3);
    i32 idx_sum = 0;
    for (const auto id : range) {
      idx_sum += v[id];
    }
    CHECK(idx_sum == 60);
  }
}

TEST_CASE("VecSlice operations", "[base][vec_slice]") {
  Vec<i32, TestIdx> v{10, 20, 30, 40, 50};

  SECTION("Slice construction and implicit conversion") {
    const VecSlice<i32, TestIdx> slice = v.slice();
    CHECK(slice.size() == 5);
    CHECK(slice.front() == 10);
    CHECK(slice.back() == 50);

    // Test implicit conversion to ConstVecSlice
    const ConstVecSlice<i32, TestIdx> const_slice = slice;
    CHECK(const_slice.size() == 5);
    CHECK(const_slice[TestIdx{2}] == 30);
  }

  SECTION("Subslice, first, and last") {
    const VecSlice<i32, TestIdx> slice = v.slice();

    auto sub = slice.subslice(1, 3);  // [20, 30, 40]
    CHECK(sub.size() == 3);
    CHECK(sub.front() == 20);
    CHECK(sub.back() == 40);

    auto first_two = slice.first(2);  // [10, 20]
    CHECK(first_two.size() == 2);
    CHECK(first_two.back() == 20);

    auto last_two = slice.last(2);  // [40, 50]
    CHECK(last_two.size() == 2);
    CHECK(last_two.front() == 40);
  }

  SECTION("pop_front and pop_back") {
    VecSlice<i32, TestIdx> slice = v.slice();

    slice.pop_front(1);
    CHECK(slice.size() == 4);
    CHECK(slice.front() == 20);

    slice.pop_back(2);
    CHECK(slice.size() == 2);
    CHECK(slice.back() == 30);
  }
}

}  // namespace base
