// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <vector>

#include "fpag/arg/parse_error.h"
#include "fpag/term/color_style.h"

namespace arg {

template <typename F>
concept ErrorFormatter = requires(const F& f,
                                  std::string_view command_name,
                                  const std::vector<ParseError>& errors,
                                  base::ColorStyle color_style) {
  { f(command_name, errors, color_style) } -> std::same_as<std::string>;
};

struct DefaultErrorFormatter {
  std::string operator()(std::string_view command_name,
                         const std::vector<ParseError>& errors,
                         base::ColorStyle color_style) const;
};

}  // namespace arg

