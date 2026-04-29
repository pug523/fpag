// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <concepts>

#include "fpag/logging/log_entry.h"

namespace logging {

template <typename Derived>
class Sink;

template <typename T>
concept IsSink = std::derived_from<T, Sink<T>>;

template <typename Derived>
class Sink {
 public:
  explicit Sink() = default;
  ~Sink() = default;

  Sink(const Sink&) = delete;
  Sink& operator=(const Sink&) = delete;

  Sink(Sink&&) noexcept = default;
  Sink& operator=(Sink&&) noexcept = default;

  void log(const LogEntry& entry) { static_cast<Derived*>(this)->log(entry); }
  void flush() { static_cast<Derived*>(this)->flush(); }
};

}  // namespace logging
