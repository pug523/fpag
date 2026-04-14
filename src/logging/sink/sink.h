// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "logging/log_entry.h"

namespace logging {

class Sink {
 public:
  Sink() = default;
  virtual ~Sink() = default;

  Sink(const Sink&) = delete;
  Sink& operator=(const Sink&) = delete;

  Sink(Sink&&) noexcept = default;
  Sink& operator=(Sink&&) noexcept = default;

  virtual void log(const LogEntry& entry) = 0;
  virtual void flush() = 0;
};

}  // namespace logging
