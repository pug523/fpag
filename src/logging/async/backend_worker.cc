// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/logging/async/backend_worker.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

#include "fpag/base/debug/check.h"
#include "fpag/base/numeric.h"
#include "fpag/base/spsc_queue.h"
#include "fpag/base/time_util.h"
#include "fpag/build/build_flag.h"
#include "fpag/logging/async/deserializer.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/sink.h"

namespace logging {

BackendWorker::BackendWorker(BackendWorker&& other) noexcept {
  swap(std::move(other));
}

BackendWorker& BackendWorker::operator=(BackendWorker&& other) noexcept {
  if (this != &other) [[likely]] {
    swap(std::move(other));
  }
  return *this;
}

void BackendWorker::swap(BackendWorker&& other) noexcept {
  std::swap(queue_, other.queue_);
  std::swap(sinks_, other.sinks_);
  std::swap(thread_, other.thread_);
  std::swap(entries_buf_, other.entries_buf_);
  std::swap(format_buf_, other.format_buf_);
  internal_status_.store(
      other.internal_status_.load(std::memory_order_acquire));
  other.internal_status_.store(InternalStatus::kNotInitialized,
                               std::memory_order_release);
}

void BackendWorker::init(usize queue_capacity, base::SpscQueue::Mode mode) {
  FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                     InternalStatus::kNotInitialized,
                     "BackendWorker is not idling");

  entries_buf_.reserve(kEntriesBufSize);
  sinks_.reserve(4);

  queue_.init(queue_capacity, mode);

  internal_status_.store(InternalStatus::kInitialized,
                         std::memory_order_release);
}

void BackendWorker::register_sink(std::unique_ptr<Sink> sink) {
  FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                     InternalStatus::kInitialized,
                     "BackendWorker is not initialized or already running");
  sinks_.push_back(std::move(sink));
}

void BackendWorker::reset() {
  if (internal_status_.load(std::memory_order_acquire) ==
      InternalStatus::kRunning) {
    stop();
  }
  internal_status_.store(InternalStatus::kNotInitialized,
                         std::memory_order_release);
  queue_.reset();
}

void BackendWorker::start() {
  FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                     InternalStatus::kInitialized,
                     "BackendWorker is not initialized or already running");
  internal_status_.store(InternalStatus::kRunning, std::memory_order_release);
  thread_ = std::make_unique<std::thread>(&BackendWorker::worker_loop, this);
}

void BackendWorker::stop() {
  FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                     InternalStatus::kRunning, "BackendWorker is not running");
  internal_status_.store(InternalStatus::kStopping, std::memory_order_release);
  if (thread_) [[likely]] {
    thread_->join();
    thread_ = nullptr;
  }
}

void BackendWorker::force_stop() {
  FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                     InternalStatus::kRunning, "BackendWorker is not running");
  // Do not flush on force stopping
  internal_status_.store(InternalStatus::kForceStopping,
                         std::memory_order_release);
  if (thread_) [[likely]] {
    thread_->join();
    thread_ = nullptr;
  }
}

void BackendWorker::flush() {
  FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                     InternalStatus::kRunning, "BackendWorker is not running");
  flush_requested_.store(true, std::memory_order_release);
  wait_for_flush();
}

void BackendWorker::worker_loop() {
  u32 count_from_last_processed = 0;
  while (true) {
    const InternalStatus status =
        internal_status_.load(std::memory_order_acquire);
    if (status == InternalStatus::kForceStopping) [[unlikely]] {
      break;
    }

    const bool processed = process_batch();

    if (flush_requested_.load(std::memory_order_acquire)) [[unlikely]] {
      flush_sinks();
      flush_requested_.store(false, std::memory_order_release);
    }
    if (status == InternalStatus::kStopping && !processed && queue_.empty())
        [[unlikely]] {
      break;
    }

    if (processed) {
      count_from_last_processed = 0;
    } else if (++count_from_last_processed >= 512) {
      wait_for_next();
    }
  }
}

bool BackendWorker::process_batch() {
  const usize queue_size = queue_.size_consumer();
  if (queue_size == 0) {
    return false;
  }

  i64 remaining_size = static_cast<i64>(queue_size);
  usize format_buf_offset = 0;

  // Precompute timestamp once per batch
  const u64 timestamp_ns = base::current_timestamp_ns();

  while (remaining_size > 0) {
    const char* const old_head = queue_.head_ptr();

    const char* const data_ptr = queue_.peek(kPayloadHeaderSize, kPayloadAlign);
    const usize payload_size = *reinterpret_cast<const usize*>(data_ptr);
    FPAG_DCHECK(reinterpret_cast<uintptr_t>(data_ptr) % 8 == 0);
    const DeserializeFunction deserializer =
        *reinterpret_cast<const DeserializeFunction*>(data_ptr + sizeof(usize));
    const LogLevel level = *reinterpret_cast<const LogLevel*>(
        data_ptr + sizeof(usize) + sizeof(DeserializeFunction));

    FPAG_DCHECK_GE(payload_size, kPayloadHeaderSize);
    FPAG_DCHECK_GE(payload_size, payload_size);

    // Deserialize args and format into format buffer.
    usize formatted_size =
        deserializer(data_ptr, payload_size, format_buf_ + format_buf_offset,
                     kFormatBufSize - format_buf_offset);

    queue_.discard(payload_size, kPayloadAlign);

    const char* const new_head = queue_.head_ptr();
    const i64 consumed_bytes = static_cast<i64>(new_head - old_head);
    FPAG_DCHECK_GE(consumed_bytes, 0);

    remaining_size -= consumed_bytes;

    const std::string_view msg{format_buf_ + format_buf_offset, formatted_size};
    entries_buf_.emplace_back(LogEntry{
        .level = level,
        .message = msg,
        .timestamp_ns = timestamp_ns,
    });
    format_buf_offset += formatted_size;

    if (format_buf_offset >= kFormatBufSize / 2 ||
        entries_buf_.size() >= kEntriesBufSize) {
      flush_entries();
      format_buf_offset = 0;
    }
  }

  flush_entries();

  return true;
}

void BackendWorker::flush_entries() {
  for (const LogEntry& entry : entries_buf_) {
    for (const std::unique_ptr<Sink>& sink : sinks_) {
      sink->log(entry);
    }
  }
  entries_buf_.clear();
}

void BackendWorker::flush_sinks() {
  for (const std::unique_ptr<Sink>& sink : sinks_) {
    sink->flush();
  }
}

void BackendWorker::wait_for_next() {
  u32 spin_count = 0;
  while (queue_.empty()) {
    if (spin_count % 32 == 0 &&
        internal_status_.load(std::memory_order_acquire) !=
            InternalStatus::kRunning) {
      return;
    }

    if (spin_count < 64) {
    } else if (spin_count < 1024) {
#if FPAG_BUILD_FLAG(IS_ARCH_X86_FAMILY) && FPAG_BUILD_FLAG(IS_COMPILER_GCC)
      __builtin_ia32_pause();
#else
      std::this_thread::yield();
#endif
    } else if (spin_count < 1024 * 1024) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128));
    } else if (spin_count < 1024 * 1024 * 1024) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128 * 1024));
    } else {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128 * 1024 * 1024));
    }
    ++spin_count;
  }
}

void BackendWorker::wait_for_flush() {
  u32 spin_count = 0;
  while (!queue_.empty()) {
    if (spin_count < 64) {
    } else if (spin_count < 1024) {
#if FPAG_BUILD_FLAG(IS_ARCH_X86_FAMILY) && FPAG_BUILD_FLAG(IS_COMPILER_GCC)
      __builtin_ia32_pause();
#else
      std::this_thread::yield();
#endif
    } else if (spin_count < 1024 * 1024) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128));
    } else if (spin_count < 1024 * 1024 * 1024) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128 * 1024));
    } else {
      std::this_thread::sleep_for(std::chrono::nanoseconds(128 * 1024 * 1024));
    }
    ++spin_count;
  }
}

}  // namespace logging
