// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <concepts>
#include <string>
#include <string_view>

#include "fpag/base/color_style.h"

namespace arg {

template <typename F>
concept VersionFormatter = requires(const F& f,
                                    std::string_view command_name,
                                    std::string_view version,
                                    base::ColorStyle color_style) {
  { f(command_name, version, color_style) } -> std::same_as<std::string>;
};

struct DefaultVersionFormatter {
  std::string operator()(std::string_view command_name,
                         std::string_view version,
                         base::ColorStyle color_style) const;
};

}  // namespace arg

