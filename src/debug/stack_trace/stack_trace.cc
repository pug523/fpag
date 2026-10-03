// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/stack_trace/stack_trace.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "fpag/base/numeric.h"
#include "fpag/debug/check.h"
#include "fpag/debug/stack_trace/capture_stack_addresses.h"
#include "fpag/debug/stack_trace/formatter.h"
#include "fpag/debug/stack_trace/stack_frame.h"
#include "fpag/debug/stack_trace/symbolicator.h"
#include "fpag/debug/string.h"
#include "fpag/io/io_util.h"

namespace debug {

void StackTrace::init(StackTraceFrame* frames_buf, usize depth, usize skip) {
  frames_ = frames_buf;
  depth_ = depth;
  skip_ = skip;

  // `FPAG_DCHECK` uses StackTrace, so to avoid infinite recursion we use the
  // raw entry points.
  //
  // The depth one is fatal in every build rather than a debug check:
  // collect_trace() captures into a fixed local array of MAX_TRACE_DEPTH, so a
  // larger depth is a stack overwrite in release, not a smaller trace.
  FPAG_RAW_CHECK_MSG(depth_ <= MAX_TRACE_DEPTH,
                     "init called with depth exceeding MAX_TRACE_DEPTH.");
  FPAG_RAW_DCHECK_MSG(skip_ <= depth_,
                      "init called with skip greater than depth.");

  status_ = StackTraceStatus::Initialized;
}

void StackTrace::collect_trace() {
  FPAG_RAW_DCHECK_MSG(status_ == StackTraceStatus::Initialized,
                      "collect_trace called on uninitialized stack trace.");

  // Capture raw addresses
  void* raw_addrs[MAX_TRACE_DEPTH];
  const usize captured = capture_stack_addresses(raw_addrs, depth_, skip_);

  if (captured == 0) [[unlikely]] {
    count_ = 0;
    status_ = StackTraceStatus::Failed;
    return;
  }

  count_ = captured;

  // The frames hold pointers into string_buffer_, so a buffer that reallocates
  // while they are being filled leaves every name interned so far dangling. A
  // worst-case reservation is a guess, and one deeply nested template name is
  // enough to overshoot it, so the names are resolved first and the buffer is
  // sized from what they actually need.
  const Symbolicator sym;
  std::vector<SymbolInfo> infos;
  infos.reserve(count_);
  for (usize i = 0; i < count_; ++i) {
    infos.push_back(sym.resolve(raw_addrs[i]));
  }

  string_buffer_.clear();
  usize needed = 0;
  for (const SymbolInfo& info : infos) {
    // One terminator per string; an empty one costs nothing, so the sum can be
    // larger than what intern_string() writes and never smaller.
    needed += info.function.size() + info.file.size() + 2;
  }
  string_buffer_.reserve(needed);

  for (usize i = 0; i < count_; ++i) {
    frames_[i].address = raw_addrs[i];
    frames_[i].index = i;

    // Intern the strings so that string_view members remain valid.
    frames_[i].location.function = intern_string(infos[i].function).data();
    frames_[i].location.file = intern_string(infos[i].file).data();
    frames_[i].location.line = infos[i].line;
    frames_[i].location.column = infos[i].column;
  }

  status_ = StackTraceStatus::Collected;
}

void StackTrace::print_trace(std::string_view prefix) const {
  if (status_ == StackTraceStatus::Failed) [[unlikely]] {
    constexpr const char* ERROR_MESSAGE = "stack trace collection failed";
    io::write(io::STDERR_FD, ERROR_MESSAGE, const_strlen(ERROR_MESSAGE));
    return;
  }

  FPAG_RAW_DCHECK_MSG(
      status_ == StackTraceStatus::Collected,
      "print_trace_with_prefix called on uncollected stack trace.");

  const std::string out = format_frames(frames_, count_, prefix);
  io::write(io::STDERR_FD, out.data(), out.size());
}

std::string StackTrace::to_string() const {
  if (status_ == StackTraceStatus::Failed) [[unlikely]] {
    return "stack trace collection failed";
  }

  FPAG_RAW_DCHECK_MSG(status_ == StackTraceStatus::Collected,
                      "to_string called on uncollected stack trace.");
  return format_frames(frames_, count_);
}
std::string_view StackTrace::intern_string(std::string_view str) {
  if (str.empty()) [[unlikely]] {
    return "";
  }
  const usize offset = string_buffer_.size();
  string_buffer_.insert(string_buffer_.end(), str.begin(), str.end());
  string_buffer_.push_back('\0');  // null-terminate for safety
  return std::string_view(string_buffer_.data() + offset, str.size());
}

void print_stack_trace_from_here() {
  std::vector<StackTraceFrame> stack_trace_buf(StackTrace::MAX_TRACE_DEPTH);
  StackTrace trace;
  trace.init(stack_trace_buf.data(), StackTrace::MAX_TRACE_DEPTH, 4);
  trace.collect_trace();
  trace.print_trace();
}

namespace {

// A signal handler has no heap, so the trace and its text live in the handler's
// own frame. Sixty four frames is what a trace is worth once the process is
// already on its way out, and the whole of it plus the text below has to fit in
// the signal stack.
constexpr usize RAW_FRAME_COUNT = 64;
// Room for a header and every address in hex, more than twice over, so the
// loop below never has to truncate a trace it could have written whole.
constexpr usize RAW_BUFFER_BYTES = 4096;
constexpr char HEX_DIGITS[] = "0123456789abcdef";

usize append_text(char* buffer, usize used, std::string_view text) noexcept {
  const usize room = RAW_BUFFER_BYTES - used;
  const usize take = text.size() < room ? text.size() : room;
  std::memcpy(buffer + used, text.data(), take);
  return used + take;
}

// Hex, because an address is not a number to read and the digits are what a
// debugger and addr2line both take.
usize append_hex(char* buffer, usize used, uintptr_t value) noexcept {
  char digits[sizeof(uintptr_t) * 2];
  usize count = 0;
  do {
    digits[count++] = HEX_DIGITS[static_cast<usize>(value & 0xF)];
    value >>= 4;
  } while (value != 0);

  used = append_text(buffer, used, "0x");
  while (count > 0) {
    const char digit[1] = {digits[--count]};
    used = append_text(buffer, used, std::string_view(digit, 1));
  }
  return used;
}

}  // namespace

void print_raw_stack_from_here() noexcept {
  // Two skipped frames: this function and the handler that called it.
  void* frames[RAW_FRAME_COUNT];
  const usize count =
      capture_stack_addresses_signal_safe(frames, RAW_FRAME_COUNT, 2);

  char buffer[RAW_BUFFER_BYTES];
  usize used = append_text(buffer, 0, "stack (raw, unresolved):\n");
  for (usize i = 0; i < count; ++i) {
    used = append_text(buffer, used, "  #");
    used = append_hex(buffer, used, i);
    used = append_text(buffer, used, "  ");
    used = append_hex(buffer, used, reinterpret_cast<uintptr_t>(frames[i]));
    used = append_text(buffer, used, "\n");

    // One write per trace where it fits, which it does; the flush is what keeps
    // a deeper one from being silently cut at the end of the buffer.
    if (used + (2 * sizeof(uintptr_t) * 2 + 8) > RAW_BUFFER_BYTES) {
      io::write(io::STDERR_FD, buffer, used);
      used = 0;
    }
  }
  io::write(io::STDERR_FD, buffer, used);
}

}  // namespace debug
