// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/signal_handler.h"

#include <cstdint>

#include "fmt/compile.h"
#include "fpag/build/build_config.h"
#include "fpag/debug/logger.h"
#include "fpag/debug/process_id.h"
#include "fpag/debug/thread_id.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#else
#error "Unsupported platform for signal handling"
#endif

#include <signal.h>

#include <chrono>
#include <csignal>
#include <ctime>
#include <string_view>

#include "fmt/chrono.h"   // IWYU pragma: keep
#include "fmt/ostream.h"  // IWYU pragma: keep
#include "fpag/base/numeric.h"
#include "fpag/debug/check.h"
#include "fpag/debug/fatal.h"
#include "fpag/debug/stack_trace/stack_trace.h"
#include "fpag/io/io_util.h"
#include "fpag/mem/page_allocator.h"

namespace debug {

namespace {

constexpr std::string_view SIGNAL_STACK_NOTE =
    "handler ran on the signal stack\n";
constexpr std::string_view THREAD_STACK_NOTE =
    "handler ran on the thread stack\n";

}  // namespace

const char* signal_to_string(i32 signal_number) {
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

  const char* sig = signal_to_string(signal_number);
  const u32 pid = current_process_id();
  const u64 tid = current_thread_id();

  DebugLogger& logger = debug_logger;
  logger.fatal(FMT_COMPILE(R"(Aborted at {:%Y-%m-%d %H:%M:%S}
({} in UNIX Time)
{} Received by PID {}  (TID {})
)"),
               tp, now, sig, pid, tid);
  // Raw addresses, and nothing else: the logger above has already used the
  // heap, and a stack overflow has left no thread stack to walk.
  print_raw_stack_from_here();

  // Which stack the handler got is the difference between a report and no
  // report, so the report says.
  const std::string_view origin =
      running_on_signal_stack() ? SIGNAL_STACK_NOTE : THREAD_STACK_NOTE;
  io::write(io::STDERR_FD, origin.data(), origin.size());

  logger.flush();
  internal::fatal_crash_impl();
}

namespace {

#if FPAG_BUILD_FLAG(IS_OS_POSIX)

// The stack the handlers run on, kept here because sigaltstack() has no query
// to ask with. A plain pointer and size: a static that runs an initializer
// would be a static constructor, which fpag does not have.
void* signal_stack = nullptr;
usize signal_stack_bytes = 0;

// Reserves the stack the handlers run on. A stack overflow is delivered while
// the thread stack is the one being overflowed, and a handler that starts there
// has none left to run on, so it would fault again before it could report
// anything. The mapping is registered with the kernel and deliberately never
// freed: a signal stack that goes away takes the next crash's report with it.
void install_signal_stack() {
  void* const stack = mem::allocate_pages(SIGNAL_STACK_BYTES);
  if (stack == nullptr) {
    // Out of memory, or an address space limit. The handlers still work, on the
    // thread stack, which is all that was ever asked of them.
    return;
  }

  // The check wants <bits/types/stack_t.h>, which is glibc's own header for
  // the type and does not exist elsewhere; <signal.h> is where POSIX puts it.
  stack_t alt_stack = {};  // NOLINTNEXTLINE(misc-include-cleaner)
  alt_stack.ss_sp = stack;
  alt_stack.ss_size = SIGNAL_STACK_BYTES;
  alt_stack.ss_flags = 0;
  if (::sigaltstack(&alt_stack, nullptr) != 0) {
    mem::free_pages(stack, SIGNAL_STACK_BYTES);
    return;
  }

  signal_stack = stack;
  signal_stack_bytes = SIGNAL_STACK_BYTES;
}

#endif  // FPAG_BUILD_FLAG(IS_OS_POSIX)

}  // namespace

#if FPAG_BUILD_FLAG(IS_OS_POSIX)

bool running_on_signal_stack() noexcept {
  if (signal_stack == nullptr) {
    return false;
  }
  // The address of a local in this frame is the only thing that can say which
  // stack the code is running on.
  const char probe = 0;
  const uintptr_t here = reinterpret_cast<uintptr_t>(&probe);
  const uintptr_t low = reinterpret_cast<uintptr_t>(signal_stack);
  return here >= low && here < low + signal_stack_bytes;
}

void register_signal_handlers() {
  install_signal_stack();

  // sigaction rather than signal(), because SA_ONSTACK is the whole point and
  // signal() cannot ask for it. SA_RESTART is what signal() installed, which is
  // how both glibc and the BSDs define it.
  const auto install = [](i32 signal_number) {
    struct sigaction action = {};
    action.sa_handler = signal_handler;
    action.sa_flags = SA_ONSTACK | SA_RESTART;
    // sa_mask is left as the aggregate initialisation made it: all bits zero,
    // which is the empty set, because signal numbers are all positive.
    // Calling sigemptyset would say so explicitly but does not compile
    // everywhere: macOS defines it as a macro expanding to `(*(set) = 0, 0)`,
    // so `::sigemptyset(x)` becomes `::(*(x) = 0, 0)` and will not parse, and
    // asking for the real function needs a feature-test macro defined before
    // any system header.
    // Only a programming error reaches here: the numbers are constants and the
    // flags are supported wherever this file compiles.
    FPAG_CHECK_MSG(::sigaction(signal_number, &action, nullptr) == 0,
                   "sigaction rejected a signal fpag installs.");
  };

  install(SIGSEGV);
  install(SIGABRT);
  install(SIGFPE);
  install(SIGILL);
  install(SIGBUS);
  install(SIGALRM);
  // SIGKILL and SIGSTOP cannot be caught at all, and SIGINT belongs to whoever
  // is driving a debugger.

#if FPAG_BUILD_FLAG(IS_DEBUG)
  install(SIGINT);
#endif
}

#else  // !FPAG_BUILD_FLAG(IS_OS_POSIX)

// There is no alternate signal stack to run on: Windows reports a fault
// through a structured exception rather than a signal, and a handler that has
// run out of thread stack has nowhere to be moved to.
bool running_on_signal_stack() noexcept {
  return false;
}

void register_signal_handlers() {
  // No sigaction either, so signal() is the whole interface. It cannot ask for
  // SA_ONSTACK, and on this platform there is no such flag to ask for.
  std::signal(SIGSEGV, signal_handler);
  std::signal(SIGABRT, signal_handler);
  std::signal(SIGFPE, signal_handler);
  std::signal(SIGILL, signal_handler);

#if FPAG_BUILD_FLAG(IS_DEBUG)
  std::signal(SIGINT, signal_handler);
#endif
}

#endif  // FPAG_BUILD_FLAG(IS_OS_POSIX)

}  // namespace debug
