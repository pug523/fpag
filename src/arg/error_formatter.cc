// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/error_formatter.h"

#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "fmt/base.h"
#include "fmt/core.h"
#include "fpag/arg/error_code.h"
#include "fpag/arg/parse_error.h"
#include "fpag/base/color_style.h"
#include "fpag/base/numeric.h"
#include "fpag/base/style.h"

namespace arg {

std::string DefaultErrorFormatter::format(std::string_view command_name,
                                          const std::vector<ParseError>& errors,
                                          base::ColorStyle style) const {
  std::string result;
  constexpr usize kEstimatedStrLenPerError = 256;
  result.reserve(kEstimatedStrLenPerError * errors.size());
  const std::back_insert_iterator<std::string> out = std::back_inserter(result);

  const char* bold = base::style_code(base::kBold, style);
  const char* bright_red = base::style_code(base::kBrightRed, style);
  const char* reset = base::style_code(base::kReset, style);

  for (const ParseError& err : errors) {
    // "error: " header
    fmt::format_to(out, "{}{}{}{}{}{} ", bright_red, bold, "error", reset, ": ",
                   bold);

    // Currently doing runtime format string parsing
    fmt::vformat_to(out, ec_to_format_str(err.code),
                    fmt::make_format_args(err.context, err.value));
  }

  // Hint
  const char* cyan = base::style_code(base::kBrightCyan, style);
  fmt::format_to(out, "\nFor more information, try '{}{} --help{}'.\n",
                 command_name, cyan, reset);
  return result;
}

}  // namespace arg
