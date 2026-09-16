// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/union.h"

#include <cstddef>
#include <type_traits>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/idx.h"
#include "fpag/base/numeric.h"

namespace base {

namespace {

struct Pair {
  u32 lo;
  u32 hi;
};

// Externally-tagged usage keeps the enclosing struct small.
struct TaggedOperand {
  Union<Idx<int, u32>, Idx<float, u32>> data;
  u8 tag;
  u8 kind;
};

}  // namespace

TEST_CASE("Union memory layout and traits", "[base][union]") {
  using IdxUnion = Union<Idx<int, u32>, Idx<float, u32>>;

  STATIC_REQUIRE(sizeof(IdxUnion) == sizeof(u32));
  STATIC_REQUIRE(alignof(IdxUnion) == alignof(u32));

  using MixedUnion = Union<u8, u32, Pair>;
  STATIC_REQUIRE(sizeof(MixedUnion) == sizeof(Pair));
  STATIC_REQUIRE(alignof(MixedUnion) == alignof(u32));

  STATIC_REQUIRE(std::is_trivially_copyable_v<IdxUnion>);
  STATIC_REQUIRE(std::is_trivially_copyable_v<MixedUnion>);
  STATIC_REQUIRE(std::is_trivially_destructible_v<MixedUnion>);

  STATIC_REQUIRE(sizeof(TaggedOperand) == 8);
  STATIC_REQUIRE(alignof(TaggedOperand) == alignof(u32));
  STATIC_REQUIRE(offsetof(TaggedOperand, data) == 0);
}

TEST_CASE("Union set and get roundtrip", "[base][union]") {
  Union<Idx<int, u32>, Idx<float, u32>> u;
  u.set(Idx<int, u32>(7));
  CHECK(u.get<Idx<int, u32>>().idx == 7);

  u.set(Idx<float, u32>(42));
  CHECK(u.get<Idx<float, u32>>().idx == 42);

  Union<u8, Pair> v;
  v.set<u8>(0xAB);
  CHECK(v.get<u8>() == 0xAB);
  v.set(Pair{.lo = 1, .hi = 2});
  CHECK(v.get<Pair>().lo == 1);
  CHECK(v.get<Pair>().hi == 2);
}

TEST_CASE("Union constexpr set and get", "[base][union]") {
  constexpr auto make = [] {
    Union<Idx<int, u32>, Idx<float, u32>> u;
    u.set(Idx<int, u32>(3));
    return u.get<Idx<int, u32>>().idx;
  };
  STATIC_REQUIRE(make() == 3);
}

}  // namespace base
