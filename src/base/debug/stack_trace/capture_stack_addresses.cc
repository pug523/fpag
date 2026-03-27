// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/stack_trace/capture_stack_addresses.h"

#include "base/numeric.h"
#include "build/attributes.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_LINUX)
#include <libunwind.h>
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
// <dbghelp.h> must be included after <windows.h>.
// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#elif FPAG_BUILD_FLAG(IS_OS_APPLE)
// macos / ios
#include <execinfo.h>
#elif FPAG_BUILD_FLAG(IS_OS_ANDROID)
#include <unwind.h>
#else
#error "Unsupported platform for stack trace capture"
#endif

namespace base {

#if FPAG_BUILD_FLAG(IS_OS_LINUX)

FPAG_NOINLINE usize capture_stack_addresses_linux(void** out_frames,
                                                  usize max_depth,
                                                  usize skip) {
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
  while (unw_step(&cursor) > 0) {
    if (skip > 0) {
      --skip;
      continue;
    }

    if (count >= max_depth) {
      break;
    }

    unw_word_t ip;
    if (unw_get_reg(&cursor, UNW_REG_IP, &ip) < 0) [[unlikely]] {
      break;  // Skip the frame if the register read fails.
    }

    if (ip == 0) [[unlikely]] {
      break;
    }

    out_frames[count++] = reinterpret_cast<void*>(ip);
  }

  return count;
}

#elif FPAG_BUILD_FLAG(IS_OS_WIN)

FPAG_NOINLINE usize capture_stack_addresses_win(void** out_frames,
                                                usize max_depth,
                                                usize skip) {
  // CaptureStackBackTrace skips `FramesToSkip` frames from the top.
  // Add 1 to also skip this function itself.
  USHORT captured = ::CaptureStackBackTrace(static_cast<ULONG>(skip),
                                            static_cast<ULONG>(max_depth),
                                            out_frames, nullptr);
  return static_cast<usize>(captured);
}

#elif FPAG_BUILD_FLAG(IS_OS_APPLE)

FPAG_NOINLINE usize capture_stack_addresses_apple(void** out_frames,
                                                  usize max_depth,
                                                  usize skip) {
  // backtrace() fills an array of void*. We allocate a temporary buffer large
  // enough to hold the skipped frames plus the frames we actually want.
  const usize total = max_depth + skip;

  // Use a stack-local VLA-equivalent via alloca to avoid a heap allocation on
  // the hot path. Cap at a reasonable size to avoid stack overflow.
  constexpr usize kMaxTotal = 512;
  void* tmp_buf[kMaxTotal];
  const usize alloc = total < kMaxTotal ? total : kMaxTotal;

  i32 captured = ::backtrace(tmp_buf, static_cast<i32>(alloc));
  if (captured <= 0) {
    return 0;
  }

  const usize offset = skip;
  if (static_cast<usize>(captured) <= offset) {
    return 0;
  }

  const usize available = static_cast<usize>(captured) - offset;
  const usize count = available < max_depth ? available : max_depth;
  for (usize i = 0; i < count; ++i) {
    out_frames[i] = tmp_buf[offset + i];
  }
  return count;
}

#elif FPAG_BUILD_FLAG(IS_OS_ANDROID)

namespace {

struct UnwindState {
  void** frames;
  usize max_depth;
  usize count;
  usize skip;
};

FPAG_NOINLINE _Unwind_Reason_Code
unwind_callback_android(struct _Unwind_Context* ctx, void* arg) {
  UnwindState* state = static_cast<UnwindState*>(arg);
  if (state->count >= state->max_depth) {
    return _URC_END_OF_STACK;
  }
  uintptr_t ip = _Unwind_GetIP(ctx);
  if (ip == 0) {
    return _URC_END_OF_STACK;
  }
  if (state->skip > 0) {
    --state->skip;
    return _URC_NO_REASON;
  }
  state->frames[state->count++] = reinterpret_cast<void*>(ip);
  return _URC_NO_REASON;
}

}  // namespace

FPAG_NOINLINE usize capture_stack_addresses_android(void** out_frames,
                                                    usize max_depth,
                                                    usize skip) {
  UnwindState state{out_frames, max_depth, 0, skip};
  _Unwind_Backtrace(unwind_callback_android, &state);
  return state.count;
}

#endif

FPAG_NOINLINE usize capture_stack_addresses(void** out_frames,
                                            usize max_depth,
                                            usize skip) {
  skip += 2;  // Skip this and the os specific wrapper.
#if FPAG_BUILD_FLAG(IS_OS_LINUX)
  return capture_stack_addresses_linux(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return capture_stack_addresses_win(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_APPLE)
  return capture_stack_addresses_apple(out_frames, max_depth, skip);
#elif FPAG_BUILD_FLAG(IS_OS_ANDROID)
  return capture_stack_addresses_android(out_frames, max_depth, skip);
#endif
}

}  // namespace base
