// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

// clang-format off
// IWYU pragma: begin_exports
#include "fpag/base/attributes.h"
#include "fpag/build/build_config.h"

#include "fpag/term/console.h"
#include "fpag/io/io_util.h"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
// #include "fpag/container/simple_concurrent_hash_map.h"
#include "fpag/container/spsc_queue.h"
#include "fpag/term/style.h"
#include "fpag/time/time_util.h"
#include "fpag/hash/xxh3_hasher.h"

#include "fpag/debug/check.h"
#include "fpag/debug/dlog.h"
#include "fpag/debug/fatal.h"
#include "fpag/debug/signal_handler.h"
#include "fpag/debug/terminate_handler.h"

#include "fpag/debug/stack_trace/stack_trace.h"

#include "fpag/logging/async/async_logger.h"
#include "fpag/logging/sync/sync_logger.h"

#include "fpag/mem/arena.h"
#include "fpag/mem/concurrent_arena.h"
#include "fpag/mem/page_allocator.h"

#include "fpag/str/string_interner.h"
#include "fpag/str/string_pool.h"
// IWYU pragma: end_exports
// clang-format on
