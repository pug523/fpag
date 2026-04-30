// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/logging/log_level.h"
#include "fpag/logging/sink/stdout_sink.h"
#include "fpag/logging/sync/sync_logger.h"

namespace base {

using DebugLogger =
    logging::SyncLogger<logging::StdoutSink, logging::LogLevel::Debug>;

DebugLogger& debug_logger();

}  // namespace base
