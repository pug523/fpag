// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "catch2/catch_session.hpp"
#define CATCH_CONFIG_RUNNER

#include "base/numeric.h"

void init() {
  // noop
}

void clean_up() {
  // noop
}

i32 main(i32 argc, char** argv) {
  init();

  Catch::Session session;

  i32 return_code = session.applyCommandLine(argc, argv);
  if (return_code != 0) {
    return return_code;
  }

  i32 result = session.run();

  clean_up();

  return result;
}
