// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "fpag/base/numeric.h"
#include "fpag/base/spsc_queue.h"
#include "fpag/logging/log_entry.h"
#include "fpag/logging/sink/sink.h"

namespace logging {

class BackendWorker {
 public:
  BackendWorker() = default;
  ~BackendWorker() = default;

  BackendWorker(const BackendWorker&) = delete;
  BackendWorker& operator=(const BackendWorker&) = delete;

  BackendWorker(BackendWorker&& other) noexcept;
  BackendWorker& operator=(BackendWorker&& other) noexcept;

  void init(usize queue_capacity = base::SpscQueue::kDefaultCapacity,
            base::SpscQueue::Mode mode = base::SpscQueue::Mode::kDefault);
  void register_sink(std::unique_ptr<Sink> sink);
  void reset();
  void start();
  void stop();
  void force_stop();
  void flush();

  inline bool running() const {
    return internal_status_.load(std::memory_order_acquire) ==
           InternalStatus::kRunning;
  }

  void swap(BackendWorker&& other) noexcept;

  inline base::SpscQueue* queue() { return &queue_; }
  inline const base::SpscQueue* queue() const { return &queue_; }

 private:
  enum class InternalStatus : u8 {
    kNotInitialized,
    kInitialized,
    kRunning,
    kStopping,
    kForceStopping,
  };

  void worker_loop();
  bool process_batch();
  void flush_entries();
  void flush_sinks();
  void wait_for_next();
  void wait_for_flush();

  base::SpscQueue queue_;
  std::vector<std::unique_ptr<Sink>> sinks_;
  std::unique_ptr<std::thread> thread_ = nullptr;
  static constexpr usize kEntriesBufSize = 64;
  std::vector<LogEntry> entries_buf_;

  // TODO: use memory buffer
  static constexpr usize kFormatBufSize = 4096;
  char format_buf_[kFormatBufSize];

  std::atomic<InternalStatus> internal_status_{InternalStatus::kNotInitialized};
  std::atomic<bool> flush_requested_{false};
};

}  // namespace logging
