// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace base {

enum class Stream : u8 {
  Stdout,
  Stderr,
};

bool is_ansi_escape_sequence_available(Stream stream);

void register_console();

}  // namespace base
