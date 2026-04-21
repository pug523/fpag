// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/signal_handler.h"

#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <unistd.h>
#else
#error "Unsupported platform for signal handling"
#endif

#if FPAG_BUILD_FLAG(USE_FMTLIB)
#include <fmt/chrono.h>   // IWYU pragma: keep
#include <fmt/ostream.h>  // IWYU pragma: keep
#include <fmt/std.h>      // IWYU pragma: keep
#endif

#include <signal.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <thread>

#include "fpag/base/debug/fatal.h"
#include "fpag/base/debug/stack_trace/stack_trace.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/sync_logger.h"

namespace base {

namespace {

inline i32 current_pid() {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  return static_cast<i32>(GetCurrentProcessId());
#else
  return getpid();
#endif
}

}  // namespace

const char* signal_to_string(int signal_number) {
  switch (signal_number) {
    case SIGSEGV: return "SIGSEGV (Invalid access to the storage)";
    case SIGABRT: return "SIGABRT (Abnormal termination)";
    case SIGFPE: return "SIGFPE (Floating point exception)";
    case SIGILL: return "SIGILL (Illegal instruction)";
    case SIGINT: return "SIGINT (Interactive attention signal)";
    case SIGTERM: return "SIGTERM (Termination request)";
#if FPAG_BUILD_FLAG(IS_OS_POSIX)
    case SIGBUS: return "SIGBUS (Bus error)";
    case SIGKILL: return "SIGKILL (Kill signal)";
    case SIGSTOP: return "SIGSTOP (Stop signal)";
    case SIGALRM: return "SIGALRM (Alarm clock)";
#endif
    default: return "Unknown signal";
  }
}

// Example signal handling output:
//
// Aborted at Thu Jan  1 00:00:00 1970
// (1234567890 in unix time)
// SIGABRT (Aborted) received by PID 12345(TID 67890)
void signal_handler(i32 signal_number) {
  const std::time_t now = std::time(nullptr);
  using std::chrono::time_point;
  const time_point<std::chrono::system_clock> tp{std::chrono::seconds(now)};

  // const std::string tid = str::format("{}", std::this_thread::get_id());
  const char* sig = signal_to_string(signal_number);
  const i32 pid = current_pid();
  const std::thread::id tid = std::this_thread::get_id();

  logging::SyncLogger& logger = logging::global_sync_logger();
  logger.fatal(R"(Aborted at {:%Y-%m-%d %H:%M:%S}
({} in UNIX Time)
{} Received by PID {}  (TID {})
)",
               tp, now, sig, pid, tid);
  print_stack_trace_from_here();

  logger.flush();
  internal::fatal_crash_impl();
}

void register_signal_handlers() {
  std::signal(SIGSEGV, signal_handler);
  std::signal(SIGABRT, signal_handler);
  std::signal(SIGFPE, signal_handler);
  std::signal(SIGILL, signal_handler);

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  std::signal(SIGBUS, signal_handler);
  std::signal(SIGKILL, signal_handler);
  std::signal(SIGSTOP, signal_handler);
  std::signal(SIGALRM, signal_handler);
#endif

#if FPAG_BUILD_FLAG(IS_DEBUG)
  std::signal(SIGINT, signal_handler);
#endif
}

}  // namespace base
