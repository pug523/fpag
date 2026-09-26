// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/logging/async/async_logger.h"

#include <string>
#include <string_view>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fmt/compile.h"
#include "fmt/ranges.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/async/codec/ref_arg.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/null_sink.h"
#include "fpag/term/console.h"
// #include "fpag/logging/sink/stdout_sink.h"
// #include "fpag/mem/page_allocator.h"

namespace logging {

namespace {

// Keeps the formatted message so a test can check what the serializer wrote
// and the deserializer read back. The backend worker appends from its own
// thread, so a test only reads the messages after joining it.
class CapturingSink {
 public:
  explicit CapturingSink(std::vector<std::string>* messages)
      : messages_(messages) {}

  CapturingSink(CapturingSink&&) noexcept = default;
  CapturingSink& operator=(CapturingSink&&) noexcept = default;
  ~CapturingSink() = default;

  void log(const LogEntry& entry) { messages_->emplace_back(entry.message); }
  void flush() {}

 private:
  std::vector<std::string>* messages_ = nullptr;
};

static_assert(Sink<CapturingSink>);

}  // namespace

TEST_CASE("AsyncLogger works correctly", "[logging][async]") {
  // AsyncLogger<StdoutSink, LogLevel::All> logger;
  // logger.init(StdoutSink(
  //     static_cast<char*>(mem::allocate_pages(mem::page_size())),
  //     mem::page_size(), term::console_color_style(term::Stream::Stdout),
  //     true));
  AsyncLogger<NullSink, LogLevel::Trace> logger;
  logger.init(NullSink{});

  logger.start_backend_worker();

  SECTION("simple logging") {
    logger.trace("async tracing");
    logger.debug("async debug log!");
    logger.info("async without formatting info");
    logger.warn("async sample warning");
    logger.error("async some error");
    logger.fatal("async fatal test");
    logger.wo_prefix("async wo prefix test");
    logger.flush();

    i32 i = 8000;
    logger.info("async formatting i32: {}", i);

    f32 f = 3.14f;
    logger.info("async formatting f32: {}", f);

    i32 color =
        static_cast<i32>(term::console_color_style(term::Stream::Stdout));
    logger.info("async color style: {}", color);

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
    logger.fatal(FMT_COMPILE("async compiled wo prefix test"));
    logger.flush();

    i32 i = 8000;
    logger.info(FMT_COMPILE("async compiled formatting i32: {}"), i);

    f32 f = 3.14f;
    logger.info(FMT_COMPILE("async compiled formatting float: {}"), f);

    i32 color =
        static_cast<i32>(term::console_color_style(term::Stream::Stdout));
    logger.info(FMT_COMPILE("async compiled color style: {}"), color);

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

TEST_CASE("AsyncLogger frames mixed fixed and dynamic arguments",
          "[logging][async]") {
  std::vector<std::string> messages;
  AsyncLogger<CapturingSink, LogLevel::Trace> logger;
  logger.init(CapturingSink{&messages});
  logger.start_backend_worker();

  const i32 count = 42;
  const std::string_view text = "text";

  // A fixed size argument next to a variable length one is where the two sides
  // have to agree on whether an argument crosses the queue with a size slot.
  logger.info("fixed then view: {} {}", count, text);
  logger.info("view then fixed: {} {}", text, count);
  logger.info("view fixed view: {} {} {}", text, count, text);
  logger.info("view vector fixed: {} {} {}", text, std::vector<i32>{1, 2},
              count);

  logger.stop_backend_worker();

  REQUIRE(messages.size() == 4);
  CHECK(messages[0] == "fixed then view: 42 text");
  CHECK(messages[1] == "view then fixed: text 42");
  CHECK(messages[2] == "view fixed view: text 42 text");
  CHECK(messages[3] == "view vector fixed: text [1, 2] 42");
}

TEST_CASE("AsyncLogger carries an argument larger than 4 KiB",
          "[logging][async]") {
  std::vector<std::string> messages;
  AsyncLogger<CapturingSink, LogLevel::Trace> logger;
  logger.init(CapturingSink{&messages}, /*interner_map_capacity=*/16 * 1024,
              /*queue_capacity=*/1 << 16);
  logger.start_backend_worker();

  constexpr usize kBigSize = 8 * 1024;
  const std::string big(kBigSize, 'x');

  // The payload is longer than the format buffer, and it only fits in the queue
  // because the caller sized the queue for it.
  logger.info("big: {}", big);

  logger.stop_backend_worker();

  REQUIRE(messages.size() == 1);
  CHECK(messages[0] == "big: " + big);
}

}  // namespace logging
