// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fmt/format.h"
#include "fpag/mem/page_allocator.h"

namespace logging {

using format_buffer = fmt::basic_memory_buffer<char, mem::kPageSize>;

}
