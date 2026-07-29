// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <concepts>
#include <string>

#include "fpag/arg/command.h"
#include "fpag/term/color_style.h"

namespace arg {

template <typename F>
concept HelpFormatter =
    requires(const F& f, const Command& command, term::ColorStyle color_style) {
      { f(command, color_style) } -> std::same_as<std::string>;
    };

struct DefaultHelpFormatter {
  std::string operator()(const Command& command,
                         term::ColorStyle color_style) const;
};

}  // namespace arg

