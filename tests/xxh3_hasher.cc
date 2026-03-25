// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/xxh3_hasher.h"

#include <string>
#include <string_view>

#include "base/numeric.h"
#include "catch2/catch_test_macros.hpp"

namespace base {

TEST_CASE("Xxh3Hasher basic functionality", "[base][hash][xxh3]") {
  Xxh3Hasher64 hasher;

  SECTION("Consistency: Same input yields same hash") {
    std::string input = "hello world";
    u64 hash1 = hasher(input);
    u64 hash2 = hasher(input);

    // Hash must be deterministic.
    CHECK(hash1 == hash2);
    CHECK(hash1 != 0);  // XXH3 for "hello world" is definitely non-zero
  }

  SECTION("Equivalence: string and string_view produce identical results") {
    std::string s = "test_string_123";
    std::string_view sv = s;

    // Both overloads should call the same underlying XXH3 function.
    CHECK(hasher(s) == hasher(sv));
  }

  SECTION("Uniqueness: Different inputs yield different hashes") {
    // Simple collision check (statistically highly unlikely for XXH3-64).
    CHECK(hasher("apple") != hasher("orange"));
    CHECK(hasher("12345") != hasher("12346"));
  }

  SECTION("Edge cases: Empty strings") {
    // Ensure empty strings are handled without crashing.
    std::string empty_s = "";
    std::string_view empty_sv = "";

    u64 hash_s = hasher(empty_s);
    u64 hash_sv = hasher(empty_sv);

    CHECK(hash_s == hash_sv);
    // XXH3 empty hash is a specific non-zero seed value.
    CHECK(hash_s != 0);
  }
}

TEST_CASE("Xxh3Hasher stability", "[base][hash]") {
  Xxh3Hasher64 hasher;

  // Known values for XXH3_64bits (No seed) to ensure the wrapper works as
  // expected These values are standard for the XXH3 algorithm
  SECTION("Verify against known empty string hash") {
    CHECK(hasher("") == 0x2D06800538D394C2ull);
  }

  SECTION("Verify against known short string hash") {
    CHECK(hasher("test") == 0x9ec9f7918d7dfc40);
    CHECK(hasher("Hello World") == 0xe34615aade2e6333);
  }
}

}  // namespace base
