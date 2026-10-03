// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/check.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <csignal>

#include "catch2/catch_message.hpp"
#include "fpag/build/build_config.h"

// A failed check is fatal by design, so the only way to observe one is from a
// parent process. The death test below needs fork(), which the wasm runtime
// does not provide even though build_config.h counts wasm as POSIX.
#if FPAG_BUILD_FLAG(IS_OS_POSIX) && !FPAG_BUILD_FLAG(IS_OS_ASMJS)

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <string>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fpag/arg/parse_result.h"
#include "fpag/base/numeric.h"
#include "fpag/base/result.h"
#include "fpag/debug/logger.h"
#include "fpag/debug/signal_handler.h"

namespace debug {

namespace {

// Runs @p operation in a forked child with the child's stderr captured into a
// pipe, and returns the child's wait status. stdout goes to /dev/null: the
// child inherits the Catch2 session, whose fatal condition handler reports the
// death through a copy of the reporter, and that report would land in the
// parent's output.
template <typename Operation>
i32 run_in_child(Operation&& operation, std::string* reported) {
  std::array<i32, 2> fds{};
  REQUIRE(::pipe(fds.data()) == 0);

  const pid_t child = ::fork();
  REQUIRE(child >= 0);

  if (child == 0) {
    // The child inherits the Catch2 session, whose fatal condition handler
    // reports the death through a copy of the reporter. That report would land
    // in the parent's output; only the library's own report matters here.
    const i32 null_fd = ::open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
      ::dup2(null_fd, STDOUT_FILENO);
      ::close(null_fd);
    }

    ::close(fds[0]);
    ::dup2(fds[1], STDERR_FILENO);
    ::close(fds[1]);
    operation();
    ::_exit(0);  // Not reached when the operation dies on a check.
  }

  ::close(fds[1]);

  std::array<char, 512> buf{};
  ssize_t got = 0;
  while ((got = ::read(fds[0], buf.data(), buf.size())) > 0) {
    reported->append(buf.data(), static_cast<usize>(got));
  }
  ::close(fds[0]);

  i32 status = 0;
  REQUIRE(::waitpid(child, &status, 0) == child);
  return status;
}

i32 run_failing_check(std::string* reported) {
  return run_in_child([] { FPAG_CHECK_MSG(false, "check_test probe"); },
                      reported);
}

// The same check with the logger the process wires up: there is a sink to
// write through, and the handlers are installed, so both the resolved trace
// and the raw one have a place to come from.
i32 run_failing_check_with_sink(std::string* reported) {
  return run_in_child(
      [] {
        init_debug_logger();
        register_signal_handlers();
        FPAG_CHECK_MSG(false, "check_test sink probe");
      },
      reported);
}

#if FPAG_BUILD_FLAG(IS_DEBUG)
// The signal a trap raises is the architecture's, not the standard's: an
// undefined instruction raises SIGILL, a breakpoint raises SIGTRAP, and the brk
// an arm64 __builtin_trap() emits is the second of those. So a child traps and
// the parent reads the signal off it, and the cases below can ask for the trap
// without a table of what each architecture calls it. Only a debug build traps,
// so only a debug build needs to know.
i32 trap_signal() {
  std::string unused;
  const i32 status = run_in_child([] { __builtin_trap(); }, &unused);
  REQUIRE(WIFSIGNALED(status));
  return WTERMSIG(status);
}
#endif  // FPAG_BUILD_FLAG(IS_DEBUG)

}  // namespace

TEST_CASE("A failed check reports without a debug logger sink",
          "[debug][check]") {
  std::string reported;
  const i32 status = run_failing_check(&reported);

  INFO(reported);
  // The report names the expression that failed and the message it was given.
  // An empty report is the stack overflow this case used to end in.
  CHECK(reported.find("'false'") != std::string::npos);
  CHECK(reported.find("check_test probe") != std::string::npos);

  // Returning at all would mean the failing check did not abort.
  const bool returned_normally = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  CHECK_FALSE(returned_normally);
  // A debug build stops on the trap instead of dying on a stack overflow. A
  // release build reaches __builtin_unreachable(), so what happens after the
  // report is undefined there by design.
#if FPAG_BUILD_FLAG(IS_DEBUG)
  const bool died_from_stack_overflow =
      WIFSIGNALED(status) && (WTERMSIG(status) == SIGSEGV);
  CHECK_FALSE(died_from_stack_overflow);
  REQUIRE(WIFSIGNALED(status));
  CHECK(WTERMSIG(status) == trap_signal());
#endif
}

// With a sink, the report is one block on one stream, and the handler that
// follows the trap does not repeat the frames it already named.
TEST_CASE("A failed check reports its trace on stderr, once",
          "[debug][check]") {
  std::string reported;
  const i32 status = run_failing_check_with_sink(&reported);

  INFO(reported);
  // The message and the trace under it are the report, and stderr is where
  // a report goes: stdout belongs to the program.
  CHECK(reported.find("check_test sink probe") != std::string::npos);
  // The resolved formatter writes a frame index as `#  0`; the raw one
  // writes it as `#0x0`.
  CHECK(reported.find("#  0") != std::string::npos);
  // The resolved trace already named the frames, so the raw list, which
  // would repeat them with less in them, stays out.
  CHECK(reported.find("stack (raw, unresolved)") == std::string::npos);

  // Returning at all would mean the failing check did not abort.
  const bool returned_normally = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  CHECK_FALSE(returned_normally);
#if FPAG_BUILD_FLAG(IS_DEBUG)
  REQUIRE(WIFSIGNALED(status));
  CHECK(WTERMSIG(status) == trap_signal());
#endif
}

TEST_CASE("Unwrapping the wrong Result tag reports in every build",
          "[base][result]") {
  std::string reported;
  const i32 status = run_in_child(
      [] {
        base::Result<i32, i32> result = base::make_err(7);
        std::move(result).unwrap();
      },
      &reported);

  INFO(reported);
  // A release build used to read the other payload without a word.
  CHECK(reported.find("'is_ok()'") != std::string::npos);
  // Returning at all would mean the unwrap gave back the other payload.
  const bool returned_normally = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  CHECK_FALSE(returned_normally);
#if FPAG_BUILD_FLAG(IS_DEBUG)
  REQUIRE(WIFSIGNALED(status));
  CHECK(WTERMSIG(status) == trap_signal());
#endif
}

TEST_CASE("Unwrapping the wrong ParseResult tag reports in every build",
          "[arg][parse_result]") {
  std::string reported;
  const i32 status = run_in_child(
      [] {
        arg::ParseResult<i32> result = arg::ParseResult<i32>::make_ok(1);
        std::move(result).unwrap_err();
      },
      &reported);

  INFO(reported);
  CHECK(reported.find("'is_err()'") != std::string::npos);
  // Returning at all would mean the unwrap gave back the other payload.
  const bool returned_normally = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  CHECK_FALSE(returned_normally);
#if FPAG_BUILD_FLAG(IS_DEBUG)
  REQUIRE(WIFSIGNALED(status));
  CHECK(WTERMSIG(status) == trap_signal());
#endif
}

}  // namespace debug

#endif  // FPAG_BUILD_FLAG(IS_OS_POSIX) && !FPAG_BUILD_FLAG(IS_OS_ASMJS)
