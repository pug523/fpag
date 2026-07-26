// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/tagged_union.h"

#include <string>
#include <string_view>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"

namespace base {

namespace {

// Helper enum class for explicit TaggedUnion testing.
enum class CustomTag : u8 {
  Int = 0,
  StringView = 1,
  Float = 2,
};

// Helper struct to track lifetime without exceptions or heap allocations.
struct MoveTracker {
  MoveTracker() = delete;
  explicit MoveTracker(i32* move_count) noexcept : move_count_(move_count) {}
  MoveTracker(const MoveTracker&) = delete;
  MoveTracker& operator=(const MoveTracker&) = delete;

  MoveTracker(MoveTracker&& other) noexcept : move_count_(other.move_count_) {
    if (move_count_ != nullptr) {
      (*move_count_)++;
    }
  }

  MoveTracker& operator=(MoveTracker&& other) noexcept {
    if (this != &other) {
      move_count_ = other.move_count_;
      if (move_count_ != nullptr) {
        (*move_count_)++;
      }
    }
    return *this;
  }

  ~MoveTracker() noexcept = default;

  i32* move_count_{nullptr};
};

// Helper struct to track destructor invocations.
struct DtorTracker {
  DtorTracker() = delete;
  explicit DtorTracker(bool* destroyed) noexcept : destroyed_(destroyed) {}
  DtorTracker(const DtorTracker&) = delete;
  DtorTracker& operator=(const DtorTracker&) = delete;

  DtorTracker(DtorTracker&& other) noexcept : destroyed_(other.destroyed_) {
    other.destroyed_ = nullptr;
  }

  DtorTracker& operator=(DtorTracker&& other) noexcept {
    if (this != &other) {
      destroyed_ = other.destroyed_;
      other.destroyed_ = nullptr;
    }
    return *this;
  }

  ~DtorTracker() noexcept {
    if (destroyed_ != nullptr) {
      *destroyed_ = true;
    }
  }

  bool* destroyed_{nullptr};
};

}  // namespace

TEST_CASE("Explicit TaggedUnion basic construction and custom enum tag",
          "[base][tagged_union]") {
  using U = TaggedUnion<CustomTag, i32, std::string_view, f64>;

  SECTION("Construct with first type (i32)") {
    U u(i32{42});
    CHECK(u.is<i32>());
    CHECK_FALSE(u.is<std::string_view>());
    CHECK_FALSE(u.is<f64>());
    CHECK(u.tag() == CustomTag::Int);
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
    Int = 1,
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

  SECTION("Move constructor") {
    const AutoTaggedUnion<MoveTracker, i32> u2(std::move(u1));
    CHECK(u2.is<MoveTracker>());
    CHECK(move_count == 2);  // 1 for temp -> u1, 1 for u1 -> u2
  }

  SECTION("Move assignment operator") {
    AutoTaggedUnion<MoveTracker, i32> u2(i32{100});
    u2 = std::move(u1);
    CHECK(u2.is<MoveTracker>());
    CHECK(move_count == 2);
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

}  // namespace base
