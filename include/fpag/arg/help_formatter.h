// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>

#include "fpag/base/color_mode.h"
#include "fpag/base/numeric.h"

namespace arg {

class Command;
class Arg;

class HelpFormatter {
 public:
  HelpFormatter() = default;
  ~HelpFormatter() = default;

  HelpFormatter(const HelpFormatter&) = delete;
  HelpFormatter& operator=(const HelpFormatter&) = delete;

  HelpFormatter(HelpFormatter&&) noexcept = default;
  HelpFormatter& operator=(HelpFormatter&&) noexcept = default;

  std::string_view format(const Command& command, base::ColorMode color_mode);
  std::string_view reformat(const Command& command, base::ColorMode color_mode);

 private:
  void render_option_line(std::string_view opt_spec,
                          usize visible_len,
                          usize max_opt_width,
                          std::string_view help_text,
                          bool is_required,
                          base::ColorMode color_mode);

  std::string formatted_str_;
};

}  // namespace arg

