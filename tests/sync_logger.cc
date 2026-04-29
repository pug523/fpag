// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/logging/sync_logger.h"

#include <string>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fmt/compile.h"
#include "fpag/base/console.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/stdout_sink.h"
#include "fpag/mem/page_allocator.h"

namespace logging {

TEST_CASE("SyncLogger works correctly", "[logging][sync]") {
  SyncLogger<StdoutSink, LogLevel::Trace> logger;
  logger.init(StdoutSink(
      static_cast<char*>(mem::allocate_pages(mem::kPageSize)), mem::kPageSize,
      base::console_color_mode(base::Stream::Stdout), true));

  SECTION("simple logging") {
    logger.trace("synced tracing");
    logger.debug("synced debug log!");
    logger.info("synced without formatting info");
    logger.warn("synced sample warning");
    logger.error("synced some error");
    logger.fatal("synced fatal test");
    logger.flush();

    i32 i = 8000;
    logger.info("formatting i32: {}", i);

    f32 f = 3.14f;
    logger.info(FMT_COMPILE("formatting float: {}"), f);

    i32 color =
        static_cast<i32>(base::console_color_mode(base::Stream::Stdout));
    logger.info(FMT_COMPILE("color mode: {}"), color);

    const char* s = "hello cstring";
    logger.info(FMT_COMPILE("formatting cstring: {}"), s);

    const std::string_view s_view = "hello string view";
    logger.info(FMT_COMPILE("formatting string_view: {}"), s_view);

    const std::string spp = "hello cpp string";
    logger.info(FMT_COMPILE("formatting c++ string: {}"), spp);

    logger.info(FMT_COMPILE("multiple args: {} {}"), i, f);

    // const i32 i_for_ref = 168;
    // logger.info(FMT_COMPILE("formatting i32 ref: {}"),
    //             logging::RefArg(i_for_ref));

    // const std::string s_for_ref = "hello ref";
    // logger.info(FMT_COMPILE("formatting ref: {}"),
    // logging::RefArg(s_for_ref));

    logger.flush();
  }
}

}  // namespace logging

