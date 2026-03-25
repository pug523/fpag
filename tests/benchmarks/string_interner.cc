// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/string_interner.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "catch2/catch_all.hpp"

namespace base {

TEST_CASE("StringInterner Benchmark", "[base][string_interner][benchmark][.]") {
  const u32 num_unique_strings = 10000;
  std::vector<std::string> test_data;
  test_data.reserve(num_unique_strings);
  for (u32 i = 0; i < num_unique_strings; ++i) {
    test_data.push_back("intern_test_string_value_" + std::to_string(i));
  }

  BENCHMARK("Interning New Strings (10k unique)") {
    // Every time, create a new Interner and measure insertion speed
    StringInterner interner(num_unique_strings * 2, 1024 * 1024, false);
    u64 checksum = 0;
    for (const auto& s : test_data) {
      checksum += interner.intern(s).chunk_id;
    }
    return checksum;
  };

  StringInterner shared_interner(num_unique_strings * 2, 1024 * 1024, false);
  for (const auto& s : test_data) {
    shared_interner.intern(s);
  }

  BENCHMARK("Interning Existing Strings (10k hits)") {
    u64 checksum = 0;
    for (const auto& s : test_data) {
      checksum += shared_interner.intern(s).chunk_id;
    }
    return checksum;
  };

  const u32 num_threads = std::thread::hardware_concurrency();
  BENCHMARK("Concurrent Interning (Mixed Hit/Miss, " +
            std::to_string(num_threads) + " threads)") {
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    StringInterner concurrent_interner(num_unique_strings * 4, 1024 * 1024,
                                       false);

    for (u32 t = 0; t < num_threads; ++t) {
      threads.emplace_back([&concurrent_interner, &test_data, t]() {
        for (u32 i = 0; i < 1000; ++i) {
          concurrent_interner.intern("shared_constant_key");
          concurrent_interner.intern(
              test_data[(t * 100 + i) % num_unique_strings]);
        }
      });
    }
    for (auto& t : threads) {
      t.join();
    }
    return concurrent_interner.size();
  };
}

}  // namespace base
