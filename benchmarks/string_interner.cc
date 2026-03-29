// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "str/string_interner.h"

#include <string>
#include <thread>
#include <vector>

#include "base/numeric.h"
#include "benchmark/benchmark.h"

namespace base {

namespace {

// PERF: optimize it

void string_interner_interning_new_strings(benchmark::State& state) {
  const u32 num_unique_strings = 10000;
  std::vector<std::string> test_data;
  test_data.reserve(num_unique_strings);
  for (u32 i = 0; i < num_unique_strings; ++i) {
    test_data.push_back("intern_test_string_value_" + std::to_string(i));
  }

  for (auto _ : state) {
    // Every time, create a new Interner and measure insertion speed
    StringInterner interner(num_unique_strings * 2, 1024 * 1024, false);
    u64 checksum = 0;
    for (const std::string& s : test_data) {
      checksum += interner.intern(s).block_id;
    }
    benchmark::DoNotOptimize(checksum);
  }
}
BENCHMARK(string_interner_interning_new_strings);

void string_interner_interning_existing_strings(benchmark::State& state) {
  const u32 num_unique_strings = 10000;
  std::vector<std::string> test_data;
  test_data.reserve(num_unique_strings);
  for (u32 i = 0; i < num_unique_strings; ++i) {
    test_data.push_back("intern_test_string_value_" + std::to_string(i));
  }

  StringInterner shared_interner(num_unique_strings * 2, 1024 * 1024, false);
  for (const std::string& s : test_data) {
    shared_interner.intern(s);
  }

  for (auto _ : state) {
    u64 checksum = 0;
    for (const std::string& s : test_data) {
      checksum += shared_interner.intern(s).block_id;
    }
    benchmark::DoNotOptimize(checksum);
  }
}
BENCHMARK(string_interner_interning_existing_strings);

void string_interner_concurrent_interning(benchmark::State& state) {
  const u32 num_unique_strings = 10000;
  std::vector<std::string> test_data;
  test_data.reserve(num_unique_strings);
  for (u32 i = 0; i < num_unique_strings; ++i) {
    test_data.push_back("intern_test_string_value_" + std::to_string(i));
  }

  const u32 num_threads = std::thread::hardware_concurrency();

  for (auto _ : state) {
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
    for (std::thread& t : threads) {
      t.join();
    }
    benchmark::DoNotOptimize(concurrent_interner.size());
  }
}
BENCHMARK(string_interner_concurrent_interning);

}  // namespace

}  // namespace base
