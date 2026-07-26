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

TEST_CASE("TaggedUnion basic construction and type checking",
          "[base][tagged_union]") {
  SECTION("Construct with integer type") {
    using U = TaggedUnion<i32, std::string_view, f64>;
    U u(i32{42});
    CHECK(u.is<i32>());
    CHECK_FALSE(u.is<std::string_view>());
    CHECK_FALSE(u.is<f64>());
    CHECK(u.tag() == U::TagOf<i32>);
    CHECK(u.tag_raw() == 0);
    CHECK(u.get<i32>() == 42);
  }

  SECTION("Construct with string_view") {
    using U = TaggedUnion<i32, std::string_view, f64>;
    U u(std::string_view{"hello, tagged_union"});
    CHECK_FALSE(u.is<i32>());
    CHECK(u.is<std::string_view>());
    CHECK_FALSE(u.is<f64>());
    CHECK(u.tag() == U::TagOf<std::string_view>);
    CHECK(u.tag_raw() == 1);
    CHECK(u.get<std::string_view>() == "hello, tagged_union");
  }
}

TEST_CASE("TaggedUnion lvalue and rvalue reference accessors",
          "[base][tagged_union]") {
  using U = TaggedUnion<i32, std::string>;
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

TEST_CASE("TaggedUnion move semantics", "[base][tagged_union]") {
  i32 move_count = 0;
  TaggedUnion<MoveTracker, i32> u1((MoveTracker(&move_count)));

  SECTION("Move constructor") {
    TaggedUnion<MoveTracker, i32> const u2(std::move(u1));
    CHECK(u2.is<MoveTracker>());
    CHECK(move_count == 2);  // 1 for temp -> u1, 1 for u1 -> u2
  }

  SECTION("Move assignment operator") {
    TaggedUnion<MoveTracker, i32> u2(i32{100});
    u2 = std::move(u1);
    CHECK(u2.is<MoveTracker>());
    CHECK(move_count == 2);
  }
}

TEST_CASE("TaggedUnion destructor propagation", "[base][tagged_union]") {
  bool destroyed = false;
  {
    TaggedUnion<i32, DtorTracker> const u((DtorTracker(&destroyed)));
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

}  // namespace base
