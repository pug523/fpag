// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/signal_handler.h"

#include "fpag/build/build_config.h"

// The handlers are the one place in fpag that runs on a stack the caller did
// not choose, with the process already dying, so what a crash report contains
// depends on which stack the handler got. Both cases here crash a child on
// purpose and read the report back.
#if FPAG_BUILD_FLAG(IS_OS_POSIX) && !FPAG_BUILD_FLAG(IS_OS_ASMJS)

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <string>
#include <string_view>

// A sanitizer replaces the signal handling this file asserts on.
// AddressSanitizer installs its own handlers and asks for the signal to be
// delivered on the thread stack, so a handler that correctly runs on the alt
// stack is a fault it cannot report, and the report comes back empty. Read
// from the compiler's own predefines rather than a build flag, because a flag
// would have to be set by every caller that sanitizes, and a caller that
// forgets it gets a red suite.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define FPAG_TEST_SIGNAL_ALT_STACK 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define FPAG_TEST_SIGNAL_ALT_STACK 1
#endif
#endif

#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/debug/logger.h"
#include "fpag/io/io_util.h"

namespace debug {

namespace {

// What the child writes when it managed to take its alternate stack away. The
// case below needs to know, because a platform that refuses the disable has no
// "no stack of its own" to report and asserting a note it cannot produce would
// be asserting the platform rather than the code.
constexpr std::string_view ALT_STACK_DISABLED = "alt stack disabled\n";

// The child: a sink for the logger, the handlers, and then a crash.
//
// The crash is raised rather than provoked by running the thread stack out. An
// overflow is what the signal stack is for, but a deliberate one does not
// survive an optimizing compiler: at -O3 the recursion becomes a loop that
// never reaches the guard page, so the case would pass or hang on the compiler
// rather than on the code. `disable_signal_stack` turns the installation off
// again, which is what makes the note in the report worth reading.
[[noreturn]] void crash_on_demand(bool disable_signal_stack) {
  init_debug_logger();
  register_signal_handlers();

  if (disable_signal_stack) {
    // As in signal_handler.cc: the check prefers glibc's own header for this.
    stack_t off = {};  // NOLINTNEXTLINE(misc-include-cleaner)
    off.ss_flags = SS_DISABLE;
    if (::sigaltstack(&off, nullptr) == 0) {
      // Falls through to the crash below with nowhere of its own to run, and
      // says so on the way past so the parent knows the case is there.
      io::write(io::STDOUT_FD, ALT_STACK_DISABLED.data(),
                ALT_STACK_DISABLED.size());
    }
  }

  ::raise(SIGSEGV);
  ::_exit(0);
}

// Runs a crashing child with both of its output streams on one pipe, and
// returns what it wrote. The child never returns: it dies in the handler.
std::string run_in_child(bool disable_signal_stack) {
  std::array<i32, 2> fds{};
  REQUIRE(::pipe(fds.data()) == 0);

  const pid_t child = ::fork();
  REQUIRE(child >= 0);

  if (child == 0) {
    ::close(fds[0]);
    ::dup2(fds[1], STDOUT_FILENO);
    ::dup2(fds[1], STDERR_FILENO);
    ::close(fds[1]);
    crash_on_demand(disable_signal_stack);
  }

  ::close(fds[1]);
  std::string reported;
  std::array<char, 512> buffer{};
  ssize_t got = 0;
  while ((got = ::read(fds[0], buffer.data(), buffer.size())) > 0) {
    reported.append(buffer.data(), static_cast<usize>(got));
  }
  ::close(fds[0]);

  i32 status = 0;
  REQUIRE(::waitpid(child, &status, 0) == child);
  return reported;
}

// The addresses in the report, one per frame, and how many hex digits each had.
#ifndef FPAG_TEST_SIGNAL_ALT_STACK
usize count_addresses(const std::string& report) {
  usize addresses = 0;
  // Each line is an index and an address, both hex; the address is the one
  // after the two spaces, because the index follows a '#'.
  for (usize at = report.find("  0x"); at != std::string::npos;
       at = report.find("  0x", at + 4)) {
    const usize first_digit = at + 4;  // past "  0x"
    usize digits = 0;
    while (first_digit + digits < report.size() && digits < 16) {
      const char digit = report[first_digit + digits];
      const bool hex =
          (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
      if (!hex) {
        break;
      }
      ++digits;
    }
    INFO("address " << addresses << " has " << digits << " hex digits");
    // Hex all the way to the end of the address: a formatter that indexed the
    // digit table with a character wrote NULs here and still looked like hex.
    REQUIRE(digits >= 8);
    ++addresses;
  }
  return addresses;
}
#endif  // FPAG_TEST_SIGNAL_ALT_STACK

}  // namespace

#ifndef FPAG_TEST_SIGNAL_ALT_STACK
TEST_CASE("The signal handler reports on the stack it was given",
          "[debug][signal_handler]") {
  // This process is on the thread stack, and says so: the note below is only
  // worth reading if the answer is not a constant.
  CHECK_FALSE(running_on_signal_stack());

  const std::string reported = run_in_child(/*disable_signal_stack=*/false);
  INFO(reported);

  CHECK(reported.find("SIGSEGV") != std::string::npos);
  CHECK(reported.find("stack (raw, unresolved):") != std::string::npos);
  // The alt stack is the point of the report: with the thread stack gone, a
  // handler running on it has nothing left to report from.
  CHECK(reported.find("handler ran on the signal stack") != std::string::npos);
  CHECK(reported.find('\0') == std::string::npos);
  CHECK(count_addresses(reported) > 3);
}
#endif  // FPAG_TEST_SIGNAL_ALT_STACK

TEST_CASE("The signal handler says when it had no stack of its own",
          "[debug][signal_handler]") {
  const std::string reported = run_in_child(/*disable_signal_stack=*/true);
  INFO(reported);

  if (reported.find(ALT_STACK_DISABLED) == std::string::npos) {
    SKIP("this platform would not take the alternate signal stack away");
  }

  // The other half of the note: with the installation undone, the handler runs
  // where it always did and the report says so, which is what tells a reader
  // that a missing report may be the missing stack.
  CHECK(reported.find("handler ran on the thread stack") != std::string::npos);
}

}  // namespace debug

#endif  // FPAG_BUILD_FLAG(IS_OS_POSIX) && !FPAG_BUILD_FLAG(IS_OS_ASMJS)
