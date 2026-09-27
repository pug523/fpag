// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fmt/format.h"
#include "fpag/base/numeric.h"

namespace logging {

constexpr usize FORMAT_BUFFER_SIZE = 4096;  // 4 KiB buffer

using format_buffer = fmt::basic_memory_buffer<char, FORMAT_BUFFER_SIZE>;

}  // namespace logging
