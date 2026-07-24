// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/version_formatter.h"

#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>

#include "fmt/base.h"
#include "fmt/core.h"

namespace arg {

std::string_view VersionFormatter::format(std::string_view command_name,
                                          std::string_view version) & {
  if (formatted_str_.empty()) {
    return reformat(command_name, version);
  }
  return formatted_str_;
}

std::string_view VersionFormatter::reformat(std::string_view command_name,
                                            std::string_view version) & {
  formatted_str_.clear();
  const std::back_insert_iterator<std::string> out =
      std::back_inserter(formatted_str_);
  fmt::format_to(out, "{} version {}", command_name, version);

  return formatted_str_;
}

}  // namespace arg

