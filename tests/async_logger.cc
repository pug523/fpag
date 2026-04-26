// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/logging/async/async_logger.h"

#include <memory>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/console.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/stdout_sink.h"
#include "fpag/mem/page_allocator.h"

namespace logging {

TEST_CASE("AsyncLogger works correctly", "[logging][async]") {
  AsyncLogger logger;
  logger.init(LogLevel::Trace);
  logger.register_sink(std::make_unique<StdoutSink>(
      static_cast<char*>(mem::allocate_pages(mem::kPageSize)), mem::kPageSize,
      base::console_color_mode(base::Stream::Stdout), true));
  logger.start_backend_worker();

  SECTION("simple logging") {
    logger.trace("tracing");
    logger.debug("debug log!");
    logger.info("without formatting info");
    logger.warn("sample warning");
    logger.error("some error");
    logger.fatal("fatal test");

    i32 i = 8000;
    logger.info("formatting i32: {}", i);

    f32 f = 3.14f;
    logger.info("formatting float: {}", f);

    i32 color =
        static_cast<i32>(base::console_color_mode(base::Stream::Stdout));
    logger.info("color mode: {}", color);

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

    logger.flush();
  }

  SECTION("clean up") {
    logger.stop_backend_worker();
  }
}

}  // namespace logging
