// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/logging/async/async_logger.h"

#include <string>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fmt/compile.h"
#include "fpag/base/console.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/async/codec/ref_arg.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/stdout_sink.h"
#include "fpag/mem/page_allocator.h"

namespace logging {

TEST_CASE("AsyncLogger works correctly", "[logging][async]") {
  AsyncLogger<StdoutSink, LogLevel::Trace> logger;
  logger.init(StdoutSink(
      static_cast<char*>(mem::allocate_pages(mem::kPageSize)), mem::kPageSize,
      base::console_color_mode(base::Stream::Stdout), true));
  // AsyncLogger<NullSink, LogLevel::Trace> logger;
  // logger.init(NullSink{});
  logger.start_backend_worker();

  SECTION("simple logging") {
    logger.trace("async tracing");
    logger.debug("async debug log!");
    logger.info("async without formatting info");
    logger.warn("async sample warning");
    logger.error("async some error");
    logger.fatal("async fatal test");
    logger.flush();

    i32 i = 8000;
    logger.info("async formatting i32: {}", i);

    f32 f = 3.14f;
    logger.info("async formatting float: {}", f);

    i32 color =
        static_cast<i32>(base::console_color_mode(base::Stream::Stdout));
    logger.info("async color mode: {}", color);

    const char* s = "hello cstring";
    logger.info("async formatting cstring: {}", s);

    const std::string_view s_view = "hello string view";
    logger.info("async formatting string_view: {}", s_view);

    const std::string spp = "hello cpp string";
    logger.info("async formatting c++ string: {}", spp);

    logger.info("async multiple args: {} {}", i, f);

    const i32 i_for_ref = 168;
    logger.info("async formatting i32 ref: {}", logging::RefArg(i_for_ref));

    const std::string s_for_ref = "hello ref";
    logger.info("async formatting ref: {}", logging::RefArg(s_for_ref));

    logger.info("async formatting multiple refs: {} {}",
                logging::RefArg(i_for_ref), logging::RefArg(s_for_ref));
    logger.flush();
  }

  SECTION("compiled format logging") {
    logger.trace(FMT_COMPILE("async compiled tracing"));
    logger.debug(FMT_COMPILE("async compiled debug log!"));
    logger.info(FMT_COMPILE("async compiled without formatting info"));
    logger.warn(FMT_COMPILE("async compiled sample warning"));
    logger.error(FMT_COMPILE("async compiled some error"));
    logger.fatal(FMT_COMPILE("async compiled fatal test"));
    logger.flush();

    i32 i = 8000;
    logger.info(FMT_COMPILE("async compiled formatting i32: {}"), i);

    f32 f = 3.14f;
    logger.info(FMT_COMPILE("async compiled formatting float: {}"), f);

    i32 color =
        static_cast<i32>(base::console_color_mode(base::Stream::Stdout));
    logger.info(FMT_COMPILE("async compiled color mode: {}"), color);

    const char* s = "hello cstring";
    logger.info(FMT_COMPILE("async compiled formatting cstring: {}"), s);

    const std::string_view s_view = "hello string view";
    logger.info(FMT_COMPILE("async compiled formatting string_view: {}"),
                s_view);

    const std::string spp = "hello cpp string";
    logger.info(FMT_COMPILE("async compiled formatting c++ string: {}"), spp);

    logger.info(FMT_COMPILE("async compiled multiple args: {} {}"), i, f);

    const i32 i_for_ref = 168;
    logger.info(FMT_COMPILE("async compiled formatting i32 ref: {}"),
                logging::RefArg(i_for_ref));

    const std::string s_for_ref = "hello ref";
    logger.info(FMT_COMPILE("async compiled formatting ref: {}"),
                logging::RefArg(s_for_ref));

    logger.info(FMT_COMPILE("async compiled formatting multiple refs: {} {}"),
                logging::RefArg(i_for_ref), logging::RefArg(s_for_ref));
    logger.flush();
  }

  SECTION("clean up") {
    logger.stop_backend_worker();
  }
}

}  // namespace logging
