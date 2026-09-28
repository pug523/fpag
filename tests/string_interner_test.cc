// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/str/string_interner.h"

#include <string>
#include <thread>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/str/string_pool_id.h"

namespace str {

namespace {

constexpr usize MAP_CAPACITY = 1024;

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
  StringInterner interner(MAP_CAPACITY);

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
  constexpr usize COUNT = 4096;

  StringInterner interner(COUNT);

  std::vector<std::string> strings;
  strings.reserve(COUNT);
  for (usize index = 0; index < COUNT; ++index) {
    strings.push_back("interned-string-" + std::to_string(index));
  }

  std::vector<StringPoolId> ids;
  intern_all(&interner, strings, &ids);

  for (usize index = 0; index < COUNT; ++index) {
    CHECK(interner.get(ids[index]) == strings[index]);
  }
  CHECK(interner.string_count() == COUNT);
}

TEST_CASE("A default-constructed interner holds names without being sized",
          "[str][interner]") {
  // The case a caller that does not know how many names it will intern is in.
  // The table takes its default size, and that size is a reservation rather
  // than an allocation, so naming nothing costs address space and not memory.
  StringInterner interner;

  const StringInterner::StringId first = interner.intern("a-name");
  const StringInterner::StringId second = interner.intern("another-name");
  const StringInterner::StringId again = interner.intern("a-name");

  CHECK(same_id(first, again));
  CHECK_FALSE(same_id(first, second));
  CHECK(interner.get(first) == "a-name");
  CHECK(interner.string_count() == 2);
}

TEST_CASE("The empty name interns to the pool's empty id", "[str][interner]") {
  StringInterner interner(MAP_CAPACITY);

  const StringInterner::StringId first = interner.intern("");
  const StringInterner::StringId again = interner.intern("");

  // The pool does not store an empty name, so its id is the empty one. What
  // matters here is that the table holds it like any other name: interning the
  // empty string twice has to give one id, or a caller that interns an empty
  // format string once per log line would grow an entry per line.
  CHECK(same_id(first, again));
  // The pool stores no bytes for an empty name, so its count does not move.
  // What matters is that the two calls agree, which is what says the table
  // holds one entry for the empty name rather than one per call.
  CHECK(interner.size() == 0);
  CHECK(interner.string_count() == 0);
}

TEST_CASE("StringInterner gives several threads one id for the same string",
          "[str][interner][threads]") {
  constexpr usize WORKER_COUNT = 4;
  constexpr usize STRING_COUNT = 256;

  StringInterner interner(STRING_COUNT);

  std::vector<std::string> strings;
  strings.reserve(STRING_COUNT);
  for (usize index = 0; index < STRING_COUNT; ++index) {
    strings.push_back("shared-string-" + std::to_string(index));
  }

  std::vector<std::vector<StringPoolId>> ids(WORKER_COUNT);
  std::vector<std::thread> workers;
  workers.reserve(WORKER_COUNT);
  for (usize worker = 0; worker < WORKER_COUNT; ++worker) {
    workers.emplace_back([&interner, &strings, &ids, worker] {
      intern_all(&interner, strings, &ids[worker]);
    });
  }
  for (std::thread& worker : workers) {
    worker.join();
  }

  // Two threads that both see a string first can both append it to the pool;
  // the map keeps one winner, so every worker must still see the same id.
  for (usize index = 0; index < STRING_COUNT; ++index) {
    for (usize worker = 0; worker < WORKER_COUNT; ++worker) {
      CHECK(same_id(ids[worker][index], ids[0][index]));
      CHECK(interner.get(ids[0][index]) == strings[index]);
    }
  }
}

}  // namespace str
