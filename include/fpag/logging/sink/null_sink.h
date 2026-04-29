// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/logging/log_entry.h"
#include "fpag/logging/sink/sink.h"

namespace logging {

class NullSink : public Sink<NullSink> {
 public:
  NullSink() = default;
  ~NullSink() = default;

  NullSink(NullSink&&) noexcept = default;
  NullSink& operator=(NullSink&&) noexcept = default;

  void log(const LogEntry&) {}
  void flush() {}
};

}  // namespace logging

