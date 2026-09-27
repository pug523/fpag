// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/idx_range.h"

#include <functional>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/idx.h"
#include "fpag/base/idx_hash.h"  // IWYU pragma: keep
#include "fpag/base/numeric.h"

namespace base {

namespace {

struct TestTag {};

using TestIdx = Idx<TestTag, u32>;

}  // namespace

TEST_CASE("IdxRange walks the range it was built from", "[base][idx_range]") {
  const IdxRange<TestIdx> range(TestIdx{4}, 3);

  CHECK(range.head() == TestIdx{4});
  CHECK(range.size() == 3);
  CHECK_FALSE(range.empty());

  std::vector<u32> values;
  for (const TestIdx id : range) {
    values.push_back(id.idx);
  }
  CHECK(values == std::vector<u32>{4, 5, 6});
}

TEST_CASE("IdxRange with no size is empty", "[base][idx_range]") {
  const IdxRange<TestIdx> range;

  CHECK(range.empty());
  CHECK(range.size() == 0);
  CHECK(range.begin() == range.end());
}

TEST_CASE("IdxRange indexes into the range", "[base][idx_range]") {
  const IdxRange<TestIdx> range(TestIdx{10}, 4);

  CHECK(range[0] == TestIdx{10});
  CHECK(range[3] == TestIdx{13});
}

TEST_CASE("IdxRange from_to includes both ends", "[base][idx_range]") {
  const IdxRange<TestIdx> range =
      IdxRange<TestIdx>::from_to(TestIdx{2}, TestIdx{5});

  CHECK(range.head() == TestIdx{2});
  CHECK(range.size() == 4);
  CHECK(range[3] == TestIdx{5});
}

TEST_CASE("Idx hashes the same value to the same hash", "[base][idx][hash]") {
  const std::hash<TestIdx> hasher;

  CHECK(hasher(TestIdx{7}) == hasher(TestIdx{7}));
  CHECK(TestIdx{7} == TestIdx{7});
  CHECK(TestIdx{7} != TestIdx{8});
}

}  // namespace base
