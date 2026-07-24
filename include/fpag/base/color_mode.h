// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"

namespace base {

enum class ColorMode : u8 {
  Auto,
  Always,
  Never,
  Unknown,
};

inline constexpr ColorMode str_to_color_mode(const std::string_view s) {
  if (s == "auto") {
    return ColorMode::Auto;
  } else if (s == "always") {
    return ColorMode::Always;
  } else if (s == "never") {
    return ColorMode::Never;
  } else {
    return ColorMode::Unknown;
  }
}

inline constexpr const char* color_mode_to_str(ColorMode c) {
  switch (c) {
    case ColorMode::Auto: return "auto";
    case ColorMode::Always: return "always";
    case ColorMode::Never: return "never";
    default: return "unknown";
  }
}

}  // namespace base

