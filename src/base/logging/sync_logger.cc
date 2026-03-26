// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/logging/sync_logger.h"

#include <algorithm>
#include <atomic>

#include "base/console.h"
#include "base/io_util.h"
#include "base/logging/log_level.h"
#include "base/mem/page_allocator.h"
#include "base/numeric.h"
#include "build/build_config.h"

#if !FPAG_BUILD_FLAG(IS_ARCH_X86_FAMILY) || !FPAG_BUILD_FLAG(IS_COMPILER_GCC)
#include <thread>
#endif

namespace base {

SyncLogger::SyncLogger(SyncLogger&& other) noexcept
    : buffer_(other.buffer_),
      capacity_(other.capacity_),
      offset_(other.offset_),
      min_level_(other.min_level_),
      use_ansi_style_(other.use_ansi_style_) {
  other.buffer_ = nullptr;
  other.offset_ = 0;
  lock_.clear();
}

SyncLogger& SyncLogger::operator=(SyncLogger&& other) noexcept {
  if (this != &other) {
    flush();

    buffer_ = other.buffer_;
    capacity_ = other.capacity_;
    offset_ = other.offset_;
    min_level_ = other.min_level_;
    use_ansi_style_ = other.use_ansi_style_;

    other.buffer_ = nullptr;
    other.offset_ = 0;

    lock_.clear();
  }
  return *this;
}

void SyncLogger::flush() {
  spin_lock();
  if (offset_ > 0) {
    write(kStdoutFd, buffer_, offset_);
    offset_ = 0;
  }
  spin_unlock();
}

void SyncLogger::write_to_shared_buffer(const char* data, usize len) {
  spin_lock();

  // Flush first if the buffer is full.
  if (offset_ + len > capacity_) {
    write(kStdoutFd, buffer_, offset_);
    offset_ = 0;

    // Directly write if the message is larger than the buffer.
    if (len > capacity_) {
      write(kStdoutFd, data, len);
      spin_unlock();
      return;
    }
  }

  std::copy_n(data, len, buffer_ + offset_);
  offset_ += len;

  spin_unlock();
}

void SyncLogger::spin_lock() {
  while (lock_.test_and_set(std::memory_order_acquire)) {
#if FPAG_BUILD_FLAG(IS_ARCH_X86_FAMILY) && FPAG_BUILD_FLAG(IS_COMPILER_GCC)
    __builtin_ia32_pause();
#else
    std::this_thread::yield();
#endif
  }
}

SyncLogger& global_logger() {
  static SyncLogger logger(static_cast<char*>(allocate_pages(kPageSize)),
                           kPageSize, LogLevel::Debug,
                           is_ansi_escape_sequence_available(Stream::Stdout));
  return logger;
}

}  // namespace base
