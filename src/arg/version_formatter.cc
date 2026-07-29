// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/version_formatter.h"

#include <cstddef>
#include <string>
#include <string_view>

#include "fpag/term/color_style.h"

namespace arg {

std::string DefaultVersionFormatter::operator()(
    std::string_view command_name,
    std::string_view version,
    term::ColorStyle /* color_style */) const {
  std::string out;
  out.reserve(command_name.size() + sizeof(" version ") + version.size());
  out.append(command_name);
  out.append(" version ");
  out.append(version);
  return out;
}

}  // namespace arg

