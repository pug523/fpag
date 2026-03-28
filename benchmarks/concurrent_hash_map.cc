// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/concurrent_hash_map.h"

#include <string>
#include <thread>
#include <vector>

#include "base/numeric.h"
#include "benchmark/benchmark.h"

namespace base {

namespace {

// PERF: optimize it

void concurrent_hash_map_insert_single_thread(benchmark::State& state) {
  const u64 capacity = 1 << 20;
  const u32 num_ops = 100000;
  for (auto _ : state) {
    ConcurrentHashMap<u64, u64> map(capacity);
    for (u32 i = 0; i < num_ops; ++i) {
      map.insert(i, i);
    }
    benchmark::DoNotOptimize(map.find(num_ops - 1));
  }
}
BENCHMARK(concurrent_hash_map_insert_single_thread);

void concurrent_hash_map_find_single_thread(benchmark::State& state) {
  const u64 capacity = 1 << 20;
  const u32 num_ops = 100000;
  ConcurrentHashMap<u64, u64> map(capacity);
  for (u32 i = 0; i < num_ops; ++i) {
    map.insert(i, i);
  }

  for (auto _ : state) {
    u64 sum = 0;
    for (u32 i = 0; i < num_ops; ++i) {
      if (const usize* v = map.find(i)) {
        sum += *v;
      }
    }
    benchmark::DoNotOptimize(sum);
  }
}
BENCHMARK(concurrent_hash_map_find_single_thread);

// TODO: add `try_insert` benchmarks

void concurrent_hash_map_concurrent_insert_multi_thread(
    benchmark::State& state) {
  const u64 capacity = 1 << 20;
  const u32 num_ops = 100000;
  const u32 num_threads = std::thread::hardware_concurrency();

  for (auto _ : state) {
    ConcurrentHashMap<u64, u64> map(capacity);
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
    benchmark::DoNotOptimize(map.size());
  }
}
BENCHMARK(concurrent_hash_map_concurrent_insert_multi_thread);

}  // namespace

}  // namespace base
