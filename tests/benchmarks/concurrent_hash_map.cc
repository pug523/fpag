// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/concurrent_hash_map.h"

#include <string>
#include <thread>
#include <vector>

#include "base/numeric.h"
#include "catch2/benchmark/catch_benchmark.hpp"
#include "catch2/catch_test_macros.hpp"

namespace base {

TEST_CASE("ConcurrentHashMap Benchmark", "[base][container][benchmark][.]") {
  const u64 capacity = 1 << 20;
  ConcurrentHashMap<u64, u64> map(capacity);

  const u32 num_ops = 100000;
  const u32 num_threads = std::thread::hardware_concurrency();

  BENCHMARK("Insert (Single-thread, 100k ops)") {
    for (u32 i = 0; i < num_ops; ++i) {
      map.insert(i, i);
    }
    return map.find(num_ops - 1);
  };

  BENCHMARK("Find (Single-thread, 100k ops)") {
    u64 sum = 0;
    for (u32 i = 0; i < num_ops; ++i) {
      if (const usize* v = map.find(i)) {
        sum += *v;
      }
    }
    return sum;
  };

  // TODO: add `try_insert` benchmarks

  BENCHMARK("Concurrent Insert (Multi-thread, " + std::to_string(num_threads) +
            " threads)") {
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (u32 i = 0; i < num_threads; ++i) {
      threads.emplace_back([&map, i]() {
        u32 start = i * num_ops;
        for (u32 j = 0; j < num_ops; ++j) {
          map.insert(start + j, j);
        }
      });
    }
    for (std::thread& t : threads) {
      t.join();
    }
    return map.size();
  };
}

}  // namespace base
