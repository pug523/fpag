// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/logging/sync/sync_logger.h"

#include <string>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fmt/compile.h"
#include "fpag/base/console.h"
#include "fpag/base/debug/dlog.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/async/codec/ref_arg.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/stdout_sink.h"
#include "fpag/mem/page_allocator.h"

namespace logging {

TEST_CASE("SyncLogger works correctly", "[logging][sync]") {
  SyncLogger<StdoutSink, LogLevel::Off> logger;
  logger.init(StdoutSink(
      static_cast<char*>(mem::allocate_pages(mem::kPageSize)), mem::kPageSize,
      base::console_color_mode(base::Stream::Stdout), true));
  // SyncLogger<NullSink, LogLevel::Off> logger;
  // logger.init(NullSink{});

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
    logger.info("formatting float: {}", f);

    i32 color =
        static_cast<i32>(base::console_color_mode(base::Stream::Stdout));
    logger.info("color mode: {}", color);

    const char* s = "hello cstring";
    logger.info("formatting cstring: {}", s);

    const std::string_view s_view = "hello string view";
    logger.info("formatting string_view: {}", s_view);

    const std::string spp = "hello cpp string";
    logger.info("formatting c++ string: {}", spp);

    logger.info("multiple args: {} {}", i, f);

    const i32 i_for_ref = 168;
    logger.info("formatting i32 ref: {}", logging::RefArg(i_for_ref));

    const std::string s_for_ref = "hello ref";
    logger.info("formatting ref: {}", logging::RefArg(s_for_ref));

    logger.flush();
  }

  SECTION("compiled format logging") {
    logger.trace(FMT_COMPILE("synced compiled tracing"));
    logger.debug(FMT_COMPILE("synced compiled debug log!"));
    logger.info(FMT_COMPILE("synced compiled without formatting info"));
    logger.warn(FMT_COMPILE("synced compiled sample warning"));
    logger.error(FMT_COMPILE("synced compiled some error"));
    logger.fatal(FMT_COMPILE("synced compiled fatal test"));
    logger.flush();

    i32 i = 8000;
    logger.info(FMT_COMPILE("formatting compiled i32: {}"), i);

    f32 f = 3.14f;
    logger.info(FMT_COMPILE("formatting compiled float: {}"), f);

    i32 color =
        static_cast<i32>(base::console_color_mode(base::Stream::Stdout));
    logger.info(FMT_COMPILE("compiled color mode: {}"), color);

    const char* s = "hello cstring";
    logger.info(FMT_COMPILE("formatting compiled cstring: {}"), s);

    const std::string_view s_view = "hello string view";
    logger.info(FMT_COMPILE("formatting compiled string_view: {}"), s_view);

    const std::string spp = "hello cpp string";
    logger.info(FMT_COMPILE("formatting compiled c++ string: {}"), spp);

    logger.info(FMT_COMPILE("multiple compiled args: {} {}"), i, f);

    const i32 i_for_ref = 168;
    logger.info(FMT_COMPILE("formatting compiled i32 ref: {}"),
                logging::RefArg(i_for_ref));

    const std::string s_for_ref = "hello ref";
    logger.info(FMT_COMPILE("formatting compiled ref: {}"),
                logging::RefArg(s_for_ref));

    logger.flush();
  }

  SECTION("dlog") {
    FPAG_DLOG("dlog testing {}", 168);
    i32 i = 8910;
    FPAG_DLOG("dlog testing2 {}", i);
  }
}

}  // namespace logging

