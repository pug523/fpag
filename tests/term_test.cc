// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fpag/term/color_mode.h"
#include "fpag/term/color_style.h"
#include "fpag/term/style.h"

namespace term {

TEST_CASE("ColorMode parses its own names", "[term]") {
  CHECK(str_to_color_mode("auto") == ColorMode::Auto);
  CHECK(str_to_color_mode("always") == ColorMode::Always);
  CHECK(str_to_color_mode("never") == ColorMode::Never);
  CHECK(str_to_color_mode("") == ColorMode::Unknown);
  CHECK(str_to_color_mode("AUTO") == ColorMode::Unknown);
}

TEST_CASE("ColorMode survives a round trip through its name", "[term]") {
  for (const ColorMode mode :
       {ColorMode::Auto, ColorMode::Always, ColorMode::Never}) {
    CHECK(str_to_color_mode(color_mode_to_str(mode)) == mode);
  }
}

TEST_CASE("A style code disappears when color is off", "[term]") {
  CHECK(std::string_view(style_code(FG_RED, ColorStyle::Off)).empty());
  CHECK(std::string_view(style_code(FG_RED, ColorStyle::Ansi16)) == FG_RED);
}

}  // namespace term
