// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <tuple>
#include <utility>

#include "fpag/logging/log_entry.h"
#include "fpag/logging/sink/sink.h"

namespace logging {

template <typename... Sinks>
class CompositeSink : public Sink<CompositeSink<Sinks...>> {
 public:
  explicit CompositeSink(Sinks&&... sinks)
      : sinks_(std::forward<Sinks>(sinks)...) {}
  ~CompositeSink() = default;

  CompositeSink(CompositeSink&&) noexcept = default;
  CompositeSink& operator=(CompositeSink&&) noexcept = default;

  void log(const LogEntry& entry) {
    std::apply([&](auto&... s) { (s.log(entry), ...); }, sinks_);
  }

  void flush() {
    std::apply([&](auto&... s) { (s.flush(), ...); }, sinks_);
  }

 private:
  std::tuple<Sinks...> sinks_;
};

}  // namespace logging
