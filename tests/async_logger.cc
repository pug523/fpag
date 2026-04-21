// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/logging/async/async_logger.h"

#include <memory>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/logging/sink/stdout_sink.h"

namespace logging {

TEST_CASE("AsyncLogger works correctly", "[logging][async]") {
  AsyncLogger logger;
  logger.init();
  logger.register_sink(std::make_unique<StdoutSink>());
  logger.start_backend_worker();

  SECTION("simple logging") {
    logger.info("without formatting");
    i32 i = 8000;
    logger.info("formatting i32: {}", i);

    f32 f = 3.14f;
    logger.info("formatting float: {}", f);

    // const char* s = "hello cstring";
    // logger.info("formatting cstring: {}", s);

    // const std::string_view s_view = "hello string view";
    // logger.info("formatting string_view: {}", s_view);

    // const std::string spp = "hello cpp string";
    // logger.info("formatting c++ string: {}", spp);

    // logger.info("multiple args: {} {}", i, f);

    // const i32 i_for_ref = 168;
    // logger.info("formatting i32 ref: {}", logging::RefArg(i_for_ref));

    // const std::string s_for_ref = "hello ref";
    // logger.info("formatting ref: {}", logging::RefArg(s_for_ref));

    logger.fatal("fatal test");
    // logger.error("e");
    // logger.warn("w");
    // logger.info("i");
    // logger.debug("d");
    // logger.trace("t");

    logger.flush();
  }

  SECTION("clean up") {
    logger.stop_backend_worker();
  }
}

}  // namespace logging
