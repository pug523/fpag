// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/container/spsc_queue.h"
#include "fpag/debug/check.h"
#include "fpag/debug/time_util.h"
#include "fpag/logging/async/deserializer.h"
#include "fpag/logging/format_buffer.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/sink.h"
#include "fpag/logging/wait_strategy.h"
#include "fpag/str/string_interner.h"

namespace logging {

template <Sink S, WaitStrategy Wait>
class BackendWorker {
 public:
  BackendWorker() = default;
  ~BackendWorker() = default;

  BackendWorker(const BackendWorker&) = delete;
  BackendWorker& operator=(const BackendWorker&) = delete;

  BackendWorker(BackendWorker&& other) noexcept { swap(std::move(other)); }
  BackendWorker& operator=(BackendWorker&& other) noexcept {
    if (this != &other) [[likely]] {
      swap(std::move(other));
    }
    return *this;
  }

  void init(
      S&& sink,
      const str::StringInterner* interner,
      usize queue_capacity = container::SpscQueue::default_capacity(),
      container::SpscQueue::Mode mode = container::SpscQueue::Mode::Default) {
    FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                       InternalStatus::NotInitialized,
                       "BackendWorker is not idling");

    sink_ = std::move(sink);
    interner_ = interner;
    queue_.init(queue_capacity, mode);

    internal_status_.store(InternalStatus::Initialized,
                           std::memory_order_release);
    FPAG_DCHECK(interner_);
  }

  void reset() {
    if (internal_status_.load(std::memory_order_acquire) ==
        InternalStatus::Running) {
      stop();
    }
    internal_status_.store(InternalStatus::NotInitialized,
                           std::memory_order_release);
    interner_ = nullptr;
    queue_.reset();
  }

  void start() {
    FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                       InternalStatus::Initialized,
                       "BackendWorker is not initialized or already running");
    internal_status_.store(InternalStatus::Running, std::memory_order_release);
    thread_ = std::make_unique<std::thread>(&BackendWorker::worker_loop, this);
  }

  void stop() {
    FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                       InternalStatus::Running, "BackendWorker is not running");
    flush();
    internal_status_.store(InternalStatus::Stopping, std::memory_order_release);
    wait_.notify();
    if (thread_) [[likely]] {
      thread_->join();
      thread_ = nullptr;
    }
  }

  void force_stop() {
    FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                       InternalStatus::Running, "BackendWorker is not running");
    // Do not flush on force stopping
    internal_status_.store(InternalStatus::ForceStopping,
                           std::memory_order_release);
    wait_.notify();
    if (thread_) [[likely]] {
      thread_->join();
      thread_ = nullptr;
    }
  }

  void flush() {
    FPAG_DCHECK_EQ_MSG(internal_status_.load(std::memory_order_acquire),
                       InternalStatus::Running, "BackendWorker is not running");
    flush_requested_.store(true, std::memory_order_release);
    wait_.notify();
    wait_for_flush();
  }

  inline bool running() const {
    return internal_status_.load(std::memory_order_acquire) ==
           InternalStatus::Running;
  }

  inline void notify_producer() { wait_.notify(); }

  // cpplint's issue: it suggests `#include <utility>` because of `swap`
  // method's name.
  // NOLINTNEXTLINE(build/include_what_you_use)
  void swap(BackendWorker&& other) noexcept {
    std::swap(queue_, other.queue_);
    std::swap(sink_, other.sink_);
    std::swap(thread_, other.thread_);
    internal_status_.store(
        other.internal_status_.load(std::memory_order_acquire));
    other.internal_status_.store(InternalStatus::NotInitialized,
                                 std::memory_order_release);
  }

  inline constexpr container::SpscQueue* queue() { return &queue_; }
  inline constexpr const container::SpscQueue* queue() const { return &queue_; }

 private:
  enum class InternalStatus : u8 {
    NotInitialized,
    Initialized,
    Running,
    Stopping,
    ForceStopping,
  };

  void worker_loop() {
    u32 count_from_last_processed = 0;
    while (true) {
      const InternalStatus status =
          internal_status_.load(std::memory_order_acquire);
      if (status == InternalStatus::ForceStopping) [[unlikely]] {
        break;
      }

      const bool processed = process_batch();

      if (flush_requested_.load(std::memory_order_acquire)) [[unlikely]] {
        sink_.flush();
        flush_requested_.store(false, std::memory_order_release);
        wait_.notify();
      }
      if (status == InternalStatus::Stopping && !processed && queue_.empty())
          [[unlikely]] {
        break;
      }

      if (processed) {
        count_from_last_processed = 0;
        wait_.reset();
      } else if (++count_from_last_processed >= 512) {
        wait_for_next();
      }
    }
  }

  bool process_batch() {
    const usize queue_size = queue_.size_consumer();
    if (queue_size == 0) {
      return false;
    }

    usize current_head = queue_.head_cache();
    const usize target_head = current_head + queue_size;

    // Precompute timestamp once per batch
    const u64 timestamp_ns = debug::current_timestamp_ns();

    while (current_head < target_head) {
      const char* const data_ptr =
          queue_.peek(kPayloadMinHeaderSize, kPayloadAlign);
      const usize payload_size = *reinterpret_cast<const usize*>(data_ptr);
      FPAG_DCHECK(reinterpret_cast<uintptr_t>(data_ptr) % 8 == 0);
      const DeserializeFunction deserializer =
          *reinterpret_cast<const DeserializeFunction*>(data_ptr +
                                                        sizeof(payload_size));
      const LogLevel level = *reinterpret_cast<const LogLevel*>(
          data_ptr + sizeof(usize) + sizeof(DeserializeFunction));

      FPAG_DCHECK_GE(payload_size, kPayloadMinHeaderSize);

      // Deserialize args and format into format buffer.
      format_buffer format_buf;
      deserializer(data_ptr, payload_size, &format_buf, interner_);

      queue_.discard(payload_size, kPayloadAlign);

      const usize aligned_head = base::round_up(current_head, kPayloadAlign);
      current_head = aligned_head + payload_size;

      const std::string_view msg{format_buf.data(), format_buf.size()};
      sink_.log(LogEntry{
          .level = level,
          .message = msg,
          .timestamp_ns = timestamp_ns,
      });
    }

    return true;
  }

  void wait_for_next() {
    std::atomic<bool> stopping{
        internal_status_.load(std::memory_order_relaxed) !=
        InternalStatus::Running};
    while (queue_.empty() && !stopping.load(std::memory_order_relaxed)) {
      wait_.wait_for_next(queue_, stopping);
      stopping.store(internal_status_.load(std::memory_order_relaxed) !=
                         InternalStatus::Running,
                     std::memory_order_relaxed);
    }
  }

  void wait_for_flush() {
    std::atomic<bool> dummy_stopping{false};
    while (!queue_.empty()) {
      wait_.wait_for_flush(queue_, dummy_stopping);
    }
  }

  container::SpscQueue queue_;
  S sink_;
  Wait wait_;
  std::unique_ptr<std::thread> thread_ = nullptr;
  const str::StringInterner* interner_ = nullptr;

  std::atomic<InternalStatus> internal_status_{InternalStatus::NotInitialized};
  std::atomic<bool> flush_requested_{false};
};

}  // namespace logging
