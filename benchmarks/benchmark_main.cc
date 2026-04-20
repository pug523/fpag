// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "benchmark/benchmark.h"
#include "fpag/base/console.h"
#include "fpag/base/debug/signal_handler.h"
#include "fpag/base/debug/terminate_handler.h"
#include "fpag/base/numeric.h"

namespace {

void init() {
  base::register_console();
  base::register_terminate_handler();
  base::register_signal_handlers();
}

}  // namespace

i32 main(i32 argc, char** argv) {
  init();

  benchmark::MaybeReenterWithoutASLR(argc, argv);
  char arg0_default[] = "benchmark";
  char* args_default = reinterpret_cast<char*>(arg0_default);
  if (!argv) {
    argc = 1;
    argv = &args_default;
  }
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
