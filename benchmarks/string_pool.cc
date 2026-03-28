// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/mem/string_pool.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "base/mem/string_pool_id.h"
#include "base/numeric.h"
#include "benchmark/benchmark.h"

namespace base {

namespace {

// PERF: optimize it

void string_pool_append_performance_pre_reserved(benchmark::State& state) {
  const u32 num_strings = 10000;
  const std::string base_str = "performance_test_string_";

  std::vector<std::string> test_data;
  test_data.reserve(num_strings);
  for (u32 i = 0; i < num_strings; ++i) {
    test_data.push_back(base_str + std::to_string(i));
  }

  for (auto _ : state) {
    StringPool pool(1024 * 1024);
    std::vector<StringPoolId> ids;
    ids.reserve(num_strings);

    for (const std::string& s : test_data) {
      ids.push_back(pool.append(s));
    }
    benchmark::DoNotOptimize(ids);
  }
}
BENCHMARK(string_pool_append_performance_pre_reserved);

void string_pool_sequential_get_10k_strings(benchmark::State& state) {
  const u32 num_strings = 10000;
  const std::string base_str = "performance_test_string_";

  std::vector<std::string> test_data;
  test_data.reserve(num_strings);
  for (u32 i = 0; i < num_strings; ++i) {
    test_data.push_back(base_str + std::to_string(i));
  }

  StringPool pool(1024 * 1024);
  std::vector<StringPoolId> ids;
  ids.reserve(test_data.size());
  for (const std::string& s : test_data) {
    ids.push_back(pool.append(s));
  }

  for (auto _ : state) {
    u64 total_len = 0;
    for (const StringPoolId& id : ids) {
      total_len += pool.get(id).length();
    }
    benchmark::DoNotOptimize(total_len);
  }
}
BENCHMARK(string_pool_sequential_get_10k_strings);

void string_pool_random_get_10k_strings(benchmark::State& state) {
  const u32 num_strings = 10000;
  const std::string base_str = "performance_test_string_";

  std::vector<std::string> test_data;
  test_data.reserve(num_strings);
  for (u32 i = 0; i < num_strings; ++i) {
    test_data.push_back(base_str + std::to_string(i));
  }

  StringPool pool(1024 * 1024);
  std::vector<StringPoolId> ids;
  ids.reserve(test_data.size());
  for (const std::string& s : test_data) {
    ids.push_back(pool.append(s));
  }

  std::vector<usize> indices(ids.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::random_device r{};
  static std::mt19937 g(r());
  std::shuffle(indices.begin(), indices.end(), g);

  for (auto _ : state) {
    u64 total_len = 0;
    for (usize idx : indices) {
      total_len += pool.get(ids[idx]).length();
    }
    benchmark::DoNotOptimize(total_len);
  }
}
BENCHMARK(string_pool_random_get_10k_strings);

}  // namespace

}  // namespace base
