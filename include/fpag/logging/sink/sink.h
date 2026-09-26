// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <concepts>

#include "fpag/logging/log_entry.h"

namespace logging {

// A Sink is a compile-time template parameter of SyncLogger and BackendWorker,
// never a runtime-polymorphic base, so every call below is statically
// dispatched and can be inlined into the caller.
//
// A Sink must be move constructible, because a logger owns its sink and hands
// it over through init(). It must NOT be default constructible: the sink is
// created by init() and does not exist before that, which is what lets a Sink
// aggregate already-configured members, e.g. CompositeSink. Move assignment is
// deliberately not required, so a sink may hold sole ownership of a resource.
template <typename T>
concept Sink = requires(T& sink, const LogEntry& entry) {
  sink.log(entry);
  sink.flush();
} && std::move_constructible<T>;

}  // namespace logging
