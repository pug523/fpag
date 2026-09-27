// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/str/string_pool.h"

#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/str/string_pool_id.h"

namespace str {

namespace {

constexpr usize POOL_CAPACITY = 1024ull * 1024;
constexpr usize STRING_SIZE = 16;

// Every string is the same length, so an offset that points at a neighbouring
// string is visible as a content mismatch rather than a length mismatch.
std::string make_string(usize worker, usize index) {
  std::string text = std::to_string(worker) + ":" + std::to_string(index);
  text.resize(STRING_SIZE, '#');
  return text;
}

// Appends strings from one worker and counts the ids that do not name the
// string the worker appended.
usize append_and_check(StringPool* pool, usize worker, usize count) {
  usize mismatches = 0;
  for (usize index = 0; index < count; ++index) {
    const std::string text = make_string(worker, index);

    std::string_view view;
    const StringPoolId id = pool->append(text, &view);
    if (view != text || pool->get(id) != text) {
      ++mismatches;
    }
  }
  return mismatches;
}

}  // namespace

TEST_CASE("StringPool hands back an id to the string it appended",
          "[str][string_pool]") {
  StringPool pool(POOL_CAPACITY);

  const std::string_view text = "interned";
  std::string_view view;
  const StringPoolId id = pool.append(text, &view);

  CHECK(view == text);
  CHECK(pool.get(id) == text);
  CHECK(pool.size() == text.size());
  CHECK(pool.string_count() == 1);

  // An empty string has no bytes to name.
  const StringPoolId empty_id = pool.append("");
  CHECK(empty_id.offset == EMPTY_STRING_ID.offset);
  CHECK(empty_id.length == EMPTY_STRING_ID.length);
  CHECK(pool.size() == text.size());
  CHECK(pool.string_count() == 1);
}

TEST_CASE("StringPool appends from several threads without crossing ids",
          "[str][string_pool][threads]") {
  constexpr usize WORKER_COUNT = 4;
  constexpr usize STRINGS_PER_WORKER = 512;

  StringPool pool(POOL_CAPACITY);

  std::vector<std::thread> workers;
  std::vector<usize> mismatches(WORKER_COUNT, 0);
  workers.reserve(WORKER_COUNT);
  for (usize worker = 0; worker < WORKER_COUNT; ++worker) {
    workers.emplace_back([&pool, &mismatches, worker] {
      mismatches[worker] = append_and_check(&pool, worker, STRINGS_PER_WORKER);
    });
  }
  for (std::thread& worker : workers) {
    worker.join();
  }

  for (usize worker = 0; worker < WORKER_COUNT; ++worker) {
    CHECK(mismatches[worker] == 0);
  }
  CHECK(pool.string_count() == WORKER_COUNT * STRINGS_PER_WORKER);
  CHECK(pool.size() == WORKER_COUNT * STRINGS_PER_WORKER * STRING_SIZE);
}

}  // namespace str
