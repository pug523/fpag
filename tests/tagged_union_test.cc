// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/tagged_union.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/testing/copy_tracker.h"
#include "fpag/testing/dtor_tracker.h"
#include "fpag/testing/move_tracker.h"

namespace base {

namespace {

// Helper enum class for explicit TaggedUnion testing.
enum class CustomTag : u8 {
  Integer = 0,
  StringView = 1,
  Float = 2,
};

}  // namespace

using testing::CopyTracker;
using testing::DtorTracker;
using testing::MoveTracker;

TEST_CASE("TaggedUnion memory layout and static checks",
          "[base][tagged_union]") {
  using SmallUnion = AutoTaggedUnion<u8, u16>;
  using MixedUnion = TaggedUnion<CustomTag, i32, std::string_view, f64>;

  STATIC_REQUIRE(sizeof(SmallUnion) ==
                 base::round_up(sizeof(u16) + sizeof(u8), alignof(u16)));
  STATIC_REQUIRE(alignof(SmallUnion) == alignof(u16));

  STATIC_REQUIRE(sizeof(MixedUnion) == sizeof(std::string_view) + sizeof(u8) +
                                           alignof(std::string_view) - 1);
  STATIC_REQUIRE(alignof(MixedUnion) == alignof(std::string_view));

  // Verify type traits propagation.
  STATIC_REQUIRE(std::is_nothrow_move_constructible_v<SmallUnion>);
  STATIC_REQUIRE(std::is_nothrow_move_assignable_v<SmallUnion>);
  STATIC_REQUIRE(std::is_copy_constructible_v<MixedUnion>);
  STATIC_REQUIRE(std::is_copy_assignable_v<MixedUnion>);

  using NonCopyableUnion = AutoTaggedUnion<MoveTracker, i32>;
  STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<NonCopyableUnion>);
  STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<NonCopyableUnion>);
}

TEST_CASE("Explicit TaggedUnion basic construction and custom enum tag",
          "[base][tagged_union]") {
  using U = TaggedUnion<CustomTag, i32, std::string_view, f64>;

  SECTION("Construct with first type (i32)") {
    U u(i32{42});
    CHECK(u.is<i32>());
    CHECK_FALSE(u.is<std::string_view>());
    CHECK_FALSE(u.is<f64>());
    CHECK(u.tag() == CustomTag::Integer);
    CHECK(u.tag() == U::TagOf<i32>);
    CHECK(u.tag_raw() == 0);
    CHECK(u.get<i32>() == 42);
  }

  SECTION("Construct with second type (std::string_view)") {
    U u(std::string_view{"explicit_tag"});
    CHECK_FALSE(u.is<i32>());
    CHECK(u.is<std::string_view>());
    CHECK_FALSE(u.is<f64>());
    CHECK(u.tag() == CustomTag::StringView);
    CHECK(u.tag() == U::TagOf<std::string_view>);
    CHECK(u.tag_raw() == 1);
    CHECK(u.get<std::string_view>() == "explicit_tag");
  }
}

TEST_CASE("Explicit TaggedUnion move assignment cleanup",
          "[base][tagged_union]") {
  enum class TrackerTag : u8 {
    Dtor = 0,
    Integer = 1,
  };
  using U = TaggedUnion<TrackerTag, DtorTracker, i32>;

  bool destroyed = false;
  U u1((DtorTracker(&destroyed)));
  U u2(i32{99});

  // Assigning u2 to u1 should invoke DtorTracker's destructor for u1's current
  // content.
  u1 = std::move(u2);
  CHECK(destroyed);
  CHECK(u1.is<i32>());
  CHECK(u1.get<i32>() == 99);
}

TEST_CASE("AutoTaggedUnion basic construction and type checking",
          "[base][tagged_union]") {
  SECTION("Construct with integer type") {
    using U = AutoTaggedUnion<i32, std::string_view, f64>;
    U u(i32{42});
    CHECK(u.is<i32>());
    CHECK_FALSE(u.is<std::string_view>());
    CHECK_FALSE(u.is<f64>());
    CHECK(u.tag() == U::TagOf<i32>);
    CHECK(u.tag_raw() == 0);
    CHECK(u.get<i32>() == 42);
  }

  SECTION("Construct with string_view") {
    using U = AutoTaggedUnion<i32, std::string_view, f64>;
    U u(std::string_view{"hello, tagged_union"});
    CHECK_FALSE(u.is<i32>());
    CHECK(u.is<std::string_view>());
    CHECK_FALSE(u.is<f64>());
    CHECK(u.tag() == U::TagOf<std::string_view>);
    CHECK(u.tag_raw() == 1);
    CHECK(u.get<std::string_view>() == "hello, tagged_union");
  }
}

TEST_CASE("AutoTaggedUnion lvalue and rvalue reference accessors",
          "[base][tagged_union]") {
  using U = AutoTaggedUnion<i32, std::string>;
  U u(std::string{"rust_style"});

  SECTION("Lvalue ref access") {
    u.get<std::string>() += "_union";
    CHECK(u.get<std::string>() == "rust_style_union");
  }

  SECTION("Const lvalue ref access") {
    const auto& const_u = u;
    CHECK(const_u.get<std::string>() == "rust_style");
  }

  SECTION("Rvalue move access") {
    std::string moved_val = std::move(u).get<std::string>();
    CHECK(moved_val == "rust_style");
  }
}

TEST_CASE("AutoTaggedUnion move semantics", "[base][tagged_union]") {
  i32 move_count = 0;
  AutoTaggedUnion<MoveTracker, i32> u1((MoveTracker(&move_count)));
  CHECK(u1.is<MoveTracker>());
  CHECK(move_count == 1);

  SECTION("Move constructor") {
    const AutoTaggedUnion<MoveTracker, i32> u2(std::move(u1));
    CHECK(u2.is<MoveTracker>());
    CHECK(move_count == 2);
  }

  SECTION("Move assignment operator") {
    AutoTaggedUnion<MoveTracker, i32> u2(i32{100});
    u2 = std::move(u1);
    CHECK(u2.is<MoveTracker>());
    CHECK(move_count == 2);
  }

  SECTION("Self move assignment") {
    AutoTaggedUnion<MoveTracker, i32>* self = &u1;
    *self = std::move(*self);
    CHECK(u1.is<MoveTracker>());
    CHECK(move_count == 1);
  }
}

TEST_CASE("AutoTaggedUnion copy semantics", "[base][tagged_union]") {
  i32 copy_count = 0;
  using U = AutoTaggedUnion<CopyTracker, i32>;
  U u1((CopyTracker(&copy_count)));

  SECTION("Copy constructor") {
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    const U u2(u1);
    CHECK(u2.is<CopyTracker>());
    CHECK(copy_count == 1);
  }

  SECTION("Copy assignment operator") {
    U u2(i32{200});
    u2 = u1;
    CHECK(u2.is<CopyTracker>());
    CHECK(copy_count == 1);
  }

  SECTION("Self copy assignment") {
    U* self = &u1;
    *self = *self;
    CHECK(u1.is<CopyTracker>());
    CHECK(copy_count == 0);
  }
}

TEST_CASE("AutoTaggedUnion destructor propagation", "[base][tagged_union]") {
  bool destroyed = false;
  {
    const AutoTaggedUnion<i32, DtorTracker> u((DtorTracker(&destroyed)));
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("TaggedUnion void layout and static checks",
          "[base][tagged_union][void]") {
  using VoidFirst = AutoTaggedUnion<void, i32>;
  using VoidSecond = AutoTaggedUnion<i32, void>;
  using VoidOnly = AutoTaggedUnion<void>;

  // A union containing only void should have size 1 (just the tag byte)
  STATIC_REQUIRE(sizeof(VoidOnly) == sizeof(u8));

  // Storage should not grow unnecessarily when paired with small types
  STATIC_REQUIRE(sizeof(VoidFirst) ==
                 base::round_up(sizeof(i32) + sizeof(u8), alignof(i32)));
  STATIC_REQUIRE(alignof(VoidFirst) == alignof(i32));

  STATIC_REQUIRE(sizeof(VoidSecond) ==
                 base::round_up(sizeof(i32) + sizeof(u8), alignof(i32)));
  STATIC_REQUIRE(alignof(VoidSecond) == alignof(i32));

  // Traits verification
  STATIC_REQUIRE(std::is_nothrow_move_constructible_v<VoidFirst>);
  STATIC_REQUIRE(std::is_nothrow_move_assignable_v<VoidFirst>);
  STATIC_REQUIRE(std::is_copy_constructible_v<VoidFirst>);
  STATIC_REQUIRE(std::is_copy_assignable_v<VoidFirst>);
}

TEST_CASE("TaggedUnion with void basic construction and inspection",
          "[base][tagged_union][void]") {
  using U = AutoTaggedUnion<void, std::string>;

  SECTION("Default construct as void") {
    const U u;  // Calls TaggedUnion() default constructor for void state
    CHECK(u.is<void>());
    CHECK_FALSE(u.is<std::string>());
    CHECK(u.tag() == U::TagOf<void>);
    CHECK(u.tag_raw() == 0);

    // Call get<void>() safely
    u.get<void>();
  }

  SECTION("Construct with non-void type (std::string)") {
    U u(std::string{"error_context"});
    CHECK_FALSE(u.is<void>());
    CHECK(u.is<std::string>());
    CHECK(u.tag() == U::TagOf<std::string>);
    CHECK(u.tag_raw() == 1);
    CHECK(u.get<std::string>() == "error_context");
  }
}

TEST_CASE("TaggedUnion with void transitions and assignments",
          "[base][tagged_union][void]") {
  using U = AutoTaggedUnion<void, DtorTracker>;

  bool destroyed = false;

  SECTION("Transition from non-void to void via move assignment") {
    U u1((DtorTracker(&destroyed)));
    CHECK(u1.is<DtorTracker>());

    U u2;  // Void variant
    u1 = std::move(u2);

    CHECK(destroyed);
    CHECK(u1.is<void>());
  }

  SECTION("Transition from void to non-void") {
    U u1;  // Void variant
    CHECK(u1.is<void>());

    U u2((DtorTracker(&destroyed)));
    u1 = std::move(u2);

    CHECK(u1.is<DtorTracker>());
    CHECK_FALSE(destroyed);
  }
}

TEST_CASE("TaggedUnion with void copy and move semantics",
          "[base][tagged_union][void]") {
  using U = AutoTaggedUnion<void, CopyTracker>;

  const i32 copy_count = 0;

  SECTION("Copy void variant") {
    const U u1;
    const U u2(u1);
    CHECK(u1.is<void>());
    CHECK(u2.is<void>());
    CHECK(copy_count == 0);
  }

  SECTION("Copy move void variant") {
    U u1;
    const U u2(std::move(u1));
    CHECK(u2.is<void>());
    CHECK(copy_count == 0);
  }
}

}  // namespace base
