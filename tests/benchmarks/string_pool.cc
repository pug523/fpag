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
#include "catch2/benchmark/catch_benchmark.hpp"
#include "catch2/catch_test_macros.hpp"

namespace base {

TEST_CASE("StringPool Benchmark", "[base][string_pool][benchmark][.]") {
  const u32 num_strings = 10000;
  const std::string base_str = "performance_test_string_";

  std::vector<std::string> test_data;
  test_data.reserve(num_strings);
  for (u32 i = 0; i < num_strings; ++i) {
    test_data.push_back(base_str + std::to_string(i));
  }

  SECTION("Append Performance") {
    BENCHMARK("Append 10k strings (Pre-reserved)") {
      StringPool pool(1024 * 1024);
      std::vector<StringPoolId> ids;
      ids.reserve(num_strings);

      for (const auto& s : test_data) {
        ids.push_back(pool.append(s));
      }
      return ids;
    };
  }

  SECTION("Access Performance") {
    StringPool pool(1024 * 1024);
    std::vector<StringPoolId> ids;
    ids.reserve(test_data.size());
    for (const auto& s : test_data) {
      ids.push_back(pool.append(s));
    }

    BENCHMARK("Sequential Get 10k strings") {
      u64 total_len = 0;
      for (const auto& id : ids) {
        total_len += pool.get(id).length();
      }
      return total_len;
    };

    std::vector<usize> indices(ids.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::random_device r{};
    static std::mt19937 g(r());
    std::shuffle(indices.begin(), indices.end(), g);

    BENCHMARK("Random Get 10k strings") {
      u64 total_len = 0;
      for (usize idx : indices) {
        total_len += pool.get(ids[idx]).length();
      }
      return total_len;
    };
  }
}

}  // namespace base
