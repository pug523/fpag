// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/stack_trace/capture_stack_addresses.h"

#include "fpag/base/attributes.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(USE_LIBUNWIND)
#include <libunwind.h>  // IWYU pragma: keep
#elif FPAG_BUILD_FLAG(IS_OS_ASMJS)
// Emscripten provides no execinfo.h; stack capture is stubbed out below.
#else
#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
// libgcc's unwinder, which the signal safe capture needs as much as Android
// does.
#include <unwind.h>
#endif
#if FPAG_BUILD_FLAG(IS_OS_ANDROID)
// Android is POSIX as far as build_config.h is concerned, and bionic has no
// execinfo.h, so the Android branch has to come first or it never runs.
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <execinfo.h>
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
// <dbghelp.h> must be included after <windows.h>.
// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#else
#error "Unsupported platform for stack trace capture"
#endif
#endif

namespace debug {

namespace {

// libgcc's unwinder reads unwind tables that are already in memory: no
// allocation and no lock, which is what a signal handler needs. Linux uses
// it only for that, and Android for everything, because bionic has no
// execinfo.h.
#if (FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)) && \
    !FPAG_BUILD_FLAG(USE_LIBUNWIND)

namespace {

struct UnwindState {
  void** frames;
  usize max_depth;
  usize count;
  usize skip;
};

FPAG_NOINLINE _Unwind_Reason_Code
unwind_callback_libgcc(struct _Unwind_Context* ctx, void* arg) {
  UnwindState* state = static_cast<UnwindState*>(arg);

  uintptr_t ip = _Unwind_GetIP(ctx);
  if (ip == 0) {
    return _URC_END_OF_STACK;
  }
  if (state->skip > 0) {
    --state->skip;
    return _URC_NO_REASON;
  }

  if (state->count >= state->max_depth) {
    return _URC_END_OF_STACK;
  }

  state->frames[state->count++] = reinterpret_cast<void*>(ip);
  return _URC_NO_REASON;
}

}  // namespace

// libgcc's unwinder, which reads unwind tables that are already in memory: no
// allocation and no lock, which is what a signal handler needs.
FPAG_NOINLINE usize capture_stack_addresses_libgcc(void** out_frames,
                                                   usize max_depth,
                                                   usize skip) {
  UnwindState state{out_frames, max_depth, 0, skip};
  _Unwind_Backtrace(unwind_callback_libgcc, &state);
  return state.count;
}

#endif

#if FPAG_BUILD_FLAG(USE_LIBUNWIND)

FPAG_NOINLINE usize capture_stack_addresses_libunwind(void** out_frames,
                                                      usize max_depth,
                                                      usize skip) {
  // NOLINTBEGIN(misc-include-cleaner)
  unw_context_t context;
  if (unw_getcontext(&context) < 0) {
    return 0;
  }
  unw_cursor_t cursor;
  if (unw_init_local(&cursor, &context) < 0) {
    return 0;
  }

  usize count = 0;
  // `unw_step` returns a positive value if there is a next frame.
  do {
    if (skip > 0) {
      --skip;
      continue;
    }
    if (count >= max_depth) {
      break;
    }

    unw_word_t ip = 0;
    if (unw_get_reg(&cursor, UNW_REG_IP, &ip) < 0 || ip == 0) {
      break;
    }

    out_frames[count++] = reinterpret_cast<void*>(ip);
  } while (unw_step(&cursor) > 0);
  // NOLINTEND(misc-include-cleaner)

  return count;
}

#elif FPAG_BUILD_FLAG(IS_OS_ASMJS)

FPAG_NOINLINE usize capture_stack_addresses_asmjs(void** out_frames,
                                                  usize max_depth,
                                                  usize skip) {
  // Emscripten provides no backtrace API; report no frames.
  (void)out_frames;
  (void)max_depth;
  (void)skip;
  return 0;
}

#elif FPAG_BUILD_FLAG(IS_OS_POSIX)

FPAG_NOINLINE usize capture_stack_addresses_posix(void** out_frames,
                                                  usize max_depth,
                                                  usize skip) {
  constexpr usize MAX_TMP = 512;
  void* tmp_buf[MAX_TMP];
  const usize fetch_count =
      (max_depth + skip) < MAX_TMP ? max_depth + skip : MAX_TMP;

  const i32 captured = ::backtrace(tmp_buf, static_cast<i32>(fetch_count));
  if (captured <= static_cast<i32>(skip)) {
    return 0;
  }

  const usize available = static_cast<usize>(captured) - skip;
  const usize count = available < max_depth ? available : max_depth;

  for (usize i = 0; i < count; ++i) {
    out_frames[i] = tmp_buf[skip + i];
  }
  return count;
}

#elif FPAG_BUILD_FLAG(IS_OS_WIN)

FPAG_NOINLINE usize capture_stack_addresses_win(void** out_frames,
                                                usize max_depth,
                                                usize skip) {
  // CaptureStackBackTrace skips `FramesToSkip` frames from the top.
  // Add 1 to also skip this function itself.
  const USHORT captured = ::CaptureStackBackTrace(static_cast<ULONG>(skip),
                                                  static_cast<ULONG>(max_depth),
                                                  out_frames, nullptr);
  return static_cast<usize>(captured);
}

#endif

}  // namespace

FPAG_NOINLINE usize capture_stack_addresses(void** out_frames,
                                            usize max_depth,
                                            usize skip) {
#if FPAG_BUILD_FLAG(USE_LIBUNWIND)
  return capture_stack_addresses_libunwind(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_ASMJS)
  return capture_stack_addresses_asmjs(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_ANDROID)
  // Ahead of POSIX for the same reason as the include: Android is POSIX, and
  // this is the branch that can compile there.
  return capture_stack_addresses_libgcc(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return capture_stack_addresses_posix(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return capture_stack_addresses_win(out_frames, max_depth, skip);
#endif
}

FPAG_NOINLINE usize capture_stack_addresses_signal_safe(void** out_frames,
                                                        usize max_depth,
                                                        usize skip) {
#if FPAG_BUILD_FLAG(USE_LIBUNWIND)
  // A local cursor walks the frames in place: nothing is resolved and nothing
  // is allocated.
  return capture_stack_addresses_libunwind(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_ASMJS)
  return capture_stack_addresses_asmjs(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
  return capture_stack_addresses_libgcc(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  // Reads the calling thread's stack and allocates nothing.
  return capture_stack_addresses_win(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  // macOS: backtrace() is in libsystem and resolves nothing on first use, so
  // there is no lazy dependency to keep away from the handler here.
  return capture_stack_addresses_posix(out_frames, max_depth, skip);
#else
  (void)out_frames;
  (void)max_depth;
  (void)skip;
  return 0;
#endif
}

}  // namespace debug
