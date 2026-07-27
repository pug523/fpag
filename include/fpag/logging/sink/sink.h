// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/logging/log_entry.h"

namespace logging {

template <typename T>
concept Sink = requires(T& sink, const LogEntry& entry) {
  sink.log(entry);
  sink.flush();
};

}  // namespace logging
