// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/str/string_interner.h"

#include <string>
#include <thread>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"

namespace str {

namespace {

constexpr usize kMapCapacity = 1024;

bool same_id(StringPoolId lhs, StringPoolId rhs) {
  return lhs.offset == rhs.offset && lhs.length == rhs.length;
}

// Interns one string list from one worker. Catch2 assertions do not run on a
// worker thread, so the worker only records the ids it was handed.
void intern_all(StringInterner* interner,
                const std::vector<std::string>& strings,
                std::vector<StringPoolId>* ids) {
  ids->reserve(strings.size());
  for (const std::string& text : strings) {
    ids->push_back(interner->intern(text));
  }
}

}  // namespace

TEST_CASE("StringInterner hands back one id per string", "[str][interner]") {
  StringInterner interner(kMapCapacity);

  const StringInterner::StringId first = interner.intern("alpha");
  const StringInterner::StringId second = interner.intern("beta");
  const StringInterner::StringId again = interner.intern("alpha");

  CHECK(same_id(first, again));
  CHECK_FALSE(same_id(first, second));
  CHECK(interner.get(first) == "alpha");
  CHECK(interner.get(second) == "beta");
  CHECK(interner.string_count() == 2);
}

TEST_CASE("StringInterner reads every string back after the pool grows",
          "[str][interner]") {
  constexpr usize kCount = 4096;

  StringInterner interner(kCount);

  std::vector<std::string> strings;
  strings.reserve(kCount);
  for (usize index = 0; index < kCount; ++index) {
    strings.push_back("interned-string-" + std::to_string(index));
  }

  std::vector<StringPoolId> ids;
  intern_all(&interner, strings, &ids);

  for (usize index = 0; index < kCount; ++index) {
    CHECK(interner.get(ids[index]) == strings[index]);
  }
  CHECK(interner.string_count() == kCount);
}

TEST_CASE("StringInterner gives several threads one id for the same string",
          "[str][interner]") {
  constexpr usize kWorkerCount = 4;
  constexpr usize kStringCount = 256;

  StringInterner interner(kStringCount);

  std::vector<std::string> strings;
  strings.reserve(kStringCount);
  for (usize index = 0; index < kStringCount; ++index) {
    strings.push_back("shared-string-" + std::to_string(index));
  }

  std::vector<std::vector<StringPoolId>> ids(kWorkerCount);
  std::vector<std::thread> workers;
  workers.reserve(kWorkerCount);
  for (usize worker = 0; worker < kWorkerCount; ++worker) {
    workers.emplace_back([&interner, &strings, &ids, worker] {
      intern_all(&interner, strings, &ids[worker]);
    });
  }
  for (std::thread& worker : workers) {
    worker.join();
  }

  // Two threads that both see a string first can both append it to the pool;
  // the map keeps one winner, so every worker must still see the same id.
  for (usize index = 0; index < kStringCount; ++index) {
    for (usize worker = 0; worker < kWorkerCount; ++worker) {
      CHECK(same_id(ids[worker][index], ids[0][index]));
      CHECK(interner.get(ids[0][index]) == strings[index]);
    }
  }
}

}  // namespace str
