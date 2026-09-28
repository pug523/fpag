// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/str/intern_table.h"

#include <atomic>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/str/string_pool.h"
#include "fpag/str/string_pool_id.h"

namespace str {

namespace {

// A hasher that returns the same value for every name, so every name lands on
// the same fingerprint. The table has to separate them by content, which is the
// one thing a fingerprint cannot do.
struct CollidingHasher {
  usize operator()(const std::string_view) const { return 0x1234; }
};

bool same_id(StringPoolId lhs, StringPoolId rhs) {
  return lhs.offset == rhs.offset && lhs.length == rhs.length;
}

}  // namespace

TEST_CASE("A table hands back one id per name", "[str][intern_table]") {
  StringPool pool;
  InternTable<> table(&pool, 1024);

  const StringPoolId first = table.intern("alpha");
  const StringPoolId second = table.intern("beta");
  const StringPoolId again = table.intern("alpha");

  CHECK(same_id(first, again));
  CHECK_FALSE(same_id(first, second));
  CHECK(pool.get(first) == "alpha");
  CHECK(pool.get(second) == "beta");
  CHECK(table.count() == 2);
}

TEST_CASE("A name that shares every fingerprint is still its own entry",
          "[str][intern_table]") {
  StringPool pool;
  InternTable<CollidingHasher> table(&pool, 64);

  // Every name has the same fingerprint, so a probe cannot reject any of them
  // and has to compare the content. With the load factor a table of 64 names
  // runs at, the names also form long probe chains.
  constexpr usize COUNT = 48;
  std::vector<StringPoolId> ids;
  ids.reserve(COUNT);
  for (usize index = 0; index < COUNT; ++index) {
    ids.push_back(table.intern("name-" + std::to_string(index)));
  }
  CHECK(table.count() == COUNT);

  for (usize index = 0; index < COUNT; ++index) {
    const std::string name = "name-" + std::to_string(index);
    const StringPoolId found = table.intern(name);
    CHECK(same_id(found, ids[index]));
    CHECK(pool.get(found) == name);
    // And a lookup, which walks the chain rather than inserting into it.
    const StringPoolId* looked_up = table.find(name);
    REQUIRE(looked_up != nullptr);
    CHECK(same_id(*looked_up, ids[index]));
  }

  // The name past the end of what was interned is still not in the table, which
  // is the case a probe that stopped at a fingerprint match would get wrong.
  CHECK(table.find("name-" + std::to_string(COUNT)) == nullptr);
  CHECK(table.count() == COUNT);
}

TEST_CASE("The empty name interns to the pool's empty id",
          "[str][intern_table]") {
  StringPool pool;
  InternTable<> table(&pool, 16);

  const StringPoolId empty = table.intern("");
  const StringPoolId again = table.intern("");

  // The pool does not store an empty name, so its id is the empty one and the
  // table holds it like any other: interning the empty name twice has to give
  // the same answer, or an interner would grow a new entry per call.
  CHECK(same_id(empty, again));
  CHECK(table.count() == 1);
}

TEST_CASE("An id stays readable after the table has taken more names",
          "[str][intern_table]") {
  constexpr usize FILLER_COUNT = 4096;

  StringPool pool;
  InternTable<> table(&pool, FILLER_COUNT + 1);
  const StringPoolId first = table.intern("first");

  for (usize index = 0; index < FILLER_COUNT; ++index) {
    table.intern("filler-" + std::to_string(index));
  }

  // The id is an offset into the pool, so it survives every name after it. An
  // interner that handed out pointers into its own table would not.
  CHECK(pool.get(first) == "first");
  const StringPoolId* looked_up = table.find("first");
  REQUIRE(looked_up != nullptr);
  CHECK(same_id(*looked_up, first));
}

TEST_CASE("A table is probed by several threads at once",
          "[str][intern_table][threads]") {
  constexpr usize WORKER_COUNT = 4;
  constexpr usize NAME_COUNT = 512;
  constexpr usize REPEATS = 8;

  StringPool pool;
  InternTable<> table(&pool, NAME_COUNT);
  std::vector<std::string> names;
  names.reserve(NAME_COUNT);
  for (usize index = 0; index < NAME_COUNT; ++index) {
    names.push_back("shared-name-" + std::to_string(index));
  }

  std::vector<std::vector<StringPoolId>> ids(WORKER_COUNT);
  std::vector<std::thread> workers;
  workers.reserve(WORKER_COUNT);
  for (usize worker = 0; worker < WORKER_COUNT; ++worker) {
    workers.emplace_back([&table, &names, &ids, worker] {
      for (usize repeat = 0; repeat < REPEATS; ++repeat) {
        for (usize index = 0; index < NAME_COUNT; ++index) {
          ids[worker].push_back(table.intern(names[index]));
        }
      }
    });
  }
  for (std::thread& worker : workers) {
    worker.join();
  }

  // Every worker has to have been handed the same id for a name, and the pool
  // has to hold one copy of it: a thread that lost the race for a slot retries
  // and finds the winner's entry rather than appending its own.
  for (usize index = 0; index < NAME_COUNT; ++index) {
    for (usize worker = 0; worker < WORKER_COUNT; ++worker) {
      CHECK(same_id(ids[worker][index], ids[0][index]));
    }
    CHECK(pool.get(ids[0][index]) == names[index]);
  }
  CHECK(table.count() == NAME_COUNT);
  CHECK(pool.string_count() == NAME_COUNT);
}

TEST_CASE("A reader finds a name while a writer is publishing it",
          "[str][intern_table][threads]") {
  constexpr usize NAME_COUNT = 4096;
  constexpr u32 SPIN_LIMIT = 200000;

  StringPool pool;
  InternTable<> table(&pool, NAME_COUNT);
  std::vector<std::string> names;
  names.reserve(NAME_COUNT);
  for (usize index = 0; index < NAME_COUNT; ++index) {
    names.push_back("published-" + std::to_string(index));
  }

  // The writer records each id in a separate array and only then says how far
  // it has got, so a name the reader looks up is a name whose slot is published
  // and whose entry is written. A reader that could see the slot before the
  // entry behind it would read an id no one ever wrote, and this is the case
  // that catches it: a suite that only reads names published long ago cannot
  // see the window at all.
  std::vector<StringPoolId> published(NAME_COUNT);
  std::atomic<usize> published_upto{0};
  std::atomic<u32> misses{0};

  std::thread writer([&] {
    for (usize index = 0; index < NAME_COUNT; ++index) {
      published[index] = table.intern(names[index]);
      published_upto.store(index + 1, std::memory_order_release);
    }
  });

  u32 spins = 0;
  while (spins < SPIN_LIMIT) {
    const usize upto = published_upto.load(std::memory_order_acquire);
    for (usize index = 0; index < upto; ++index) {
      const StringPoolId* found = table.find(names[index]);
      if (found == nullptr || !same_id(*found, published[index])) {
        misses.fetch_add(1, std::memory_order_relaxed);
      }
    }
    if (upto == NAME_COUNT) {
      break;
    }
    ++spins;
  }
  writer.join();

  // A name the writer has published has to be findable, every time. The reader
  // may not have looked yet, which is what the spin limit is for; it may not
  // have found a *different* id for it, which is what the counter is for.
  CHECK(misses.load(std::memory_order_relaxed) == 0);
  CHECK(table.count() == NAME_COUNT);
}

TEST_CASE("A default-sized table holds a compiler's share of names",
          "[str][intern_table]") {
  StringPool pool;
  // The point of the default: a caller that does not know how many names it
  // will intern does not have to say, and the region it reserves is address
  // space rather than memory. A thousand names must not be a special case.
  InternTable<> table(&pool);

  CHECK(table.capacity() >= InternTable<>::DEFAULT_NAMES);
  for (usize index = 0; index < 1000; ++index) {
    const StringPoolId id = table.intern("name-" + std::to_string(index));
    CHECK(pool.get(id) == "name-" + std::to_string(index));
  }
  CHECK(table.count() == 1000);
  // Nine bytes a slot is the whole claim, and it is what a compiler's symbol
  // table costs: the entry it replaces was 32.
  CHECK(table.allocated_bytes() <=
        table.capacity() * (1 + sizeof(StringPoolId)));
}

}  // namespace str
