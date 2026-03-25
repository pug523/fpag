// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/string_pool.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/numeric.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

namespace base {

TEST_CASE("StringPool basic operations", "[base][string_pool]") {
  StringPool pool(1024);

  SECTION("Append and get") {
    std::string_view original = "hello_world";
    StringPoolId id = pool.append(original);
    CHECK(pool.string_count() > 0);
    CHECK(pool.size() > 0);

    std::string_view stored = pool.get(id);
    CHECK(stored == original);
    CHECK(stored.length() == original.length());
    CHECK(pool.string_count() > 0);
    CHECK(pool.size() > 0);
  }

  SECTION("Empty string handling") {
    StringPoolId id = pool.append("");
    CHECK(pool.string_count() == 0);
    CHECK(pool.size() == 0);

    CHECK(pool.get(id).empty());
    CHECK(id.length == 0);
  }

  SECTION("Multiple appends maintain integrity") {
    StringPoolId id1 = pool.append("first");
    StringPoolId id2 = pool.append("second_string");
    StringPoolId id3 = pool.append("third");
    CHECK(pool.string_count() > 0);
    CHECK(pool.size() > 0);

    CHECK(pool.get(id1) == "first");
    CHECK(pool.get(id2) == "second_string");
    CHECK(pool.get(id3) == "third");
  }
}

TEST_CASE("StringPool memory and capacity", "[base][string_pool]") {
  // Start with small capacity and trigger expansion
  StringPool pool(16);

  SECTION("Reserve updates capacity") {
    pool.reserve(2048);
    StringPoolId id = pool.append("checking_reserve_integrity");
    CHECK(pool.string_count() > 0);
    CHECK(pool.size() > 0);
    CHECK(pool.get(id) == "checking_reserve_integrity");
  }

  SECTION("Reset clears the pool") {
    pool.append("data");
    pool.append("more_data");
    CHECK(pool.string_count() > 0);
    CHECK(pool.size() > 0);

    pool.reset();
    CHECK(pool.size() == 0);

    StringPoolId id = pool.append("new_start");
    CHECK(pool.get(id) == "new_start");
  }
}

TEST_CASE("StringPool stress and boundary checks", "[base][string_pool]") {
  StringPool pool(64);

  SECTION("Large number of strings") {
    std::vector<StringPoolId> ids;
    constexpr usize k = 1000;
    ids.reserve(k);
    for (u32 i = 0; i < k; ++i) {
      ids.push_back(pool.append("string_" + std::to_string(i)));
    }

    for (u32 i = 0; i < k; ++i) {
      CHECK(pool.get(ids[i]) == "string_" + std::to_string(i));
    }
  }

  SECTION("Very long string") {
    std::string long_str(4096, 'a');
    StringPoolId id = pool.append(long_str);
    CHECK(pool.get(id) == long_str);
  }
}

TEST_CASE("StringPool move semantics", "[base][string_pool]") {
  StringPool original_pool(1024);

  // Setup: Add some data to the original pool
  std::string_view content = "data_to_be_moved";
  StringPoolId id = original_pool.append(content);
  usize original_size = original_pool.size();

  SECTION("Move Constructor") {
    // Perform move.
    StringPool moved_to_pool(std::move(original_pool));

    // Verify data is preserved in the new pool.
    CHECK(moved_to_pool.get(id) == content);
    CHECK(moved_to_pool.size() == original_size);

    // Verify source pool is in a valid, likely empty state.
    // NOLINTNEXTLINE(bugprone-use-after-move)
    CHECK(original_pool.size() == 0);

    // Verify the new pool can still be used.
    StringPoolId new_id = moved_to_pool.append("new_data");
    CHECK(moved_to_pool.get(new_id) == "new_data");
  }

  SECTION("Move Assignment") {
    StringPool assigned_pool(16);  // Small initial capacity
    assigned_pool.append("discard_me");

    // Perform move assignment.
    assigned_pool = std::move(original_pool);

    // Verify data from original is now in assigned_pool.
    CHECK(assigned_pool.get(id) == content);
    CHECK(assigned_pool.size() == original_size);

    // Verify original is reset.
    // NOLINTNEXTLINE(bugprone-use-after-move)
    CHECK(original_pool.size() == 0);

    // Verify pointers/IDs from the original pool are still valid in the new
    // one.
    CHECK(assigned_pool.get(id).data() != nullptr);
  }
}

}  // namespace base
