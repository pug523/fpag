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
#include "fpag/base/numeric.h"

namespace debug {

namespace {

// Runs a failing check in a forked child with the child's stderr captured into
// a pipe, and returns the child's wait status. The child never calls
// init_debug_logger(), which is the case being covered: a check that fails
// before the debug logger has a sink.
i32 run_failing_check(std::string* reported) {
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
    FPAG_CHECK_MSG(false, "check_test probe");
    ::_exit(0);  // Not reached: the check above does not return.
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

  // The process stops on the trap instead of dying on that stack overflow.
  const bool died_from_stack_overflow =
      WIFSIGNALED(status) && (WTERMSIG(status) == SIGSEGV);
  CHECK_FALSE(died_from_stack_overflow);
#if FPAG_BUILD_FLAG(IS_DEBUG)
  REQUIRE(WIFSIGNALED(status));
  CHECK(WTERMSIG(status) == SIGILL);
#endif
}

}  // namespace debug

#endif  // FPAG_BUILD_FLAG(IS_OS_POSIX)
