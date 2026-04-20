// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/stack_trace/stack_trace.h"

#include <string>
#include <vector>

#include "fpag/base/debug/check.h"
#include "fpag/base/debug/stack_trace/capture_stack_addresses.h"
#include "fpag/base/debug/stack_trace/formatter.h"
#include "fpag/base/debug/stack_trace/stack_frame.h"
#include "fpag/base/debug/stack_trace/symbolicator.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/sync_logger.h"

namespace base {

void StackTrace::init(StackTraceFrame* frames_buf, usize depth, usize skip) {
  frames_ = frames_buf;
  depth_ = depth;
  skip_ = skip;

  // `FPAG_DCHECK` uses StackTrace, so to avoid infinite recursion we use
  // `raw_FPAG_DCHECK_msg`.
  FPAG_RAW_DCHECK_MSG(depth_ <= kMaxTraceDepth,
                      "init called with depth exceeding kMaxTraceDepth.");
  FPAG_RAW_DCHECK_MSG(skip_ <= depth_,
                      "init called with skip greater than depth.");

  status_ = StackTraceStatus::Initialized;
}

void StackTrace::collect_trace() {
  FPAG_RAW_DCHECK_MSG(status_ == StackTraceStatus::Initialized,
                      "collect_trace called on uninitialized stack trace.");

  // Capture raw addresses
  void* raw_addrs[kMaxTraceDepth];
  const usize captured = capture_stack_addresses(raw_addrs, depth_, skip_);

  if (captured == 0) [[unlikely]] {
    status_ = StackTraceStatus::Failed;
    return;
  }

  count_ = captured;

  // Symbolicate
  // Pre-allocate string_buffer_ to avoid reallocations that would invalidate
  // string_view pointers. Worst case: 256 chars per frame (function + file).
  string_buffer_.reserve(count_ * 256);

  Symbolicator sym;
  for (usize i = 0; i < count_; ++i) {
    frames_[i].address = raw_addrs[i];
    frames_[i].index = i;

    SymbolInfo info = sym.resolve(raw_addrs[i]);

    // Intern the strings so that string_view members remain valid.
    frames_[i].function = intern_string(info.function);
    frames_[i].file = intern_string(info.file);
    frames_[i].line = info.line;
    frames_[i].column = info.column;
  }

  status_ = StackTraceStatus::Collected;
}

void StackTrace::print_trace(std::string_view prefix) const {
  logging::SyncLogger& logger = logging::global_sync_logger();

  if (status_ == StackTraceStatus::Failed) [[unlikely]] {
    logger.error("stack trace collection failed");
    return;
  }

  FPAG_RAW_DCHECK_MSG(
      status_ == StackTraceStatus::Collected,
      "print_trace_with_prefix called on uncollected stack trace.");

  const std::string out = format_frames(frames_, count_, prefix);
  logger.info("\n{}", out);
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
  std::vector<StackTraceFrame> stack_trace_buf(StackTrace::kMaxTraceDepth);
  StackTrace trace;
  trace.init(stack_trace_buf.data(), StackTrace::kMaxTraceDepth, 4);
  trace.collect_trace();
  trace.print_trace();
}

}  // namespace base
