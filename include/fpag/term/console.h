// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"
#include "fpag/term/color_mode.h"
#include "fpag/term/color_style.h"

namespace term {

enum class Stream : u8 {
  Stdout,
  Stderr,
};

bool is_ansi_available(Stream stream);

ColorStyle console_color_style(Stream stream,
                               ColorMode color_mode = ColorMode::Auto);

void register_console();

}  // namespace term
