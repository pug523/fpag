// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "fpag/base/color_style.h"
#include "fpag/base/numeric.h"

namespace arg {

class Command;

class HelpFormatter {
 public:
  HelpFormatter() = default;
  ~HelpFormatter() = default;

  HelpFormatter(const HelpFormatter&) = delete;
  HelpFormatter& operator=(const HelpFormatter&) = delete;

  HelpFormatter(HelpFormatter&&) noexcept = default;
  HelpFormatter& operator=(HelpFormatter&&) noexcept = default;

  std::string_view format(const Command& parser,
                          base::ColorStyle color_style) &;
  std::string_view reformat(const Command& parser,
                            base::ColorStyle color_style) &;

  inline std::string&& format(const Command& parser,
                              base::ColorStyle color_style) && {
    format(parser, color_style);
    return std::move(formatted_str_);
  }
  inline std::string&& reformat(const Command& parser,
                                base::ColorStyle color_style) && {
    reformat(parser, color_style);
    return std::move(formatted_str_);
  }

 private:
  void render_option_line(std::string_view opt_spec,
                          usize visible_len,
                          usize max_opt_width,
                          std::string_view help_text,
                          bool is_required,
                          base::ColorStyle color_style);

  std::string formatted_str_;
};

}  // namespace arg

