// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

// clang-format off
// IWYU pragma: begin_exports
#include "fpag/build/attributes.h"
#include "fpag/build/build_config.h"

#include "fpag/base/console.h"
#include "fpag/base/io_util.h"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
// #include "fpag/base/simple_concurrent_hash_map.h"
#include "fpag/base/spsc_queue.h"
#include "fpag/base/style.h"
#include "fpag/base/time_util.h"
#include "fpag/base/xxh3_hasher.h"

#include "fpag/base/debug/check.h"
#include "fpag/base/debug/dlog.h"
#include "fpag/base/debug/fatal.h"
#include "fpag/base/debug/signal_handler.h"
#include "fpag/base/debug/terminate_handler.h"

#include "fpag/base/debug/stack_trace/stack_trace.h"

#include "fpag/logging/async/async_logger.h"
#include "fpag/logging/sync_logger.h"

#include "fpag/mem/arena.h"
#include "fpag/mem/concurrent_arena.h"
#include "fpag/mem/page_allocator.h"

#include "fpag/str/format_util.h"
#include "fpag/str/string_interner.h"
#include "fpag/str/string_pool.h"
// IWYU pragma: end_exports
// clang-format on
