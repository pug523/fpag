// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/check.h"

#include "fpag/build/build_config.h"

// A failed check is fatal by design, so the only way to observe one is from a
// parent process. The death test below is POSIX only.
#if FPAG_BUILD_FLAG(IS_OS_POSIX)

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "fpag/arg/parse_result.h"
#include "fpag/base/numeric.h"
#include "fpag/base/result.h"

namespace debug {

namespace {

// Runs @p operation in a forked child with the child's stderr captured into a
// pipe, and returns the child's wait status. The child never calls
// init_debug_logger(), which is the case being covered: a check that fails
// before the debug logger has a sink.
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
  CHECK(WTERMSIG(status) == SIGILL);
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
  CHECK(WTERMSIG(status) == SIGILL);
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
  CHECK(WTERMSIG(status) == SIGILL);
#endif
}

}  // namespace debug

#endif  // FPAG_BUILD_FLAG(IS_OS_POSIX)
