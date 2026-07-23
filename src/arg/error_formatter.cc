// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/error_formatter.h"

#include <iterator>
#include <string_view>
#include <vector>

#include "fmt/base.h"
#include "fmt/core.h"
#include "fpag/arg/error_code.h"
#include "fpag/arg/parse_error.h"
#include "fpag/base/color_mode.h"
#include "fpag/base/style.h"

namespace arg {

namespace {

inline const char* s(const char* code, base::ColorMode mode) noexcept {
  return base::style_code(code, mode);
}

}  // namespace

std::string_view ErrorFormatter::format(const std::vector<ParseError>& errors,
                                        std::string_view command_name,
                                        base::ColorMode color_mode) {
  formatted_str_.clear();

  // Use a slightly larger estimate for multiple errors
  formatted_str_.reserve(256 * errors.size());
  auto out = std::back_inserter(formatted_str_);

  for (const auto& err : errors) {
    // "error: " header
    fmt::format_to(out, "{}{}{} ", s(base::kBrightRed, color_mode),
                   s(base::kBold, color_mode), "error:");
    // Detaileconst d error message
    switch (err.code) {
      case ErrorCode::InvalidArgCount:
        fmt::format_to(out, "invalid arg count provided: {}", err.value);
        break;
      case ErrorCode::NullMatchesPointer:
        fmt::format_to(out, "output matches pointer is null");
        break;
      case ErrorCode::UnknownLongOption:
        fmt::format_to(out, "unexpected argument '--{}' found", err.context,
                       err.value);
        break;
      case ErrorCode::UnknownShortOption:
        fmt::format_to(out, "unexpected argument '-{}' found", err.context);
        break;
      case ErrorCode::MissingValueForOption:
        fmt::format_to(out,
                       "a value is required for '{}' but none was supplied",
                       err.context);
        break;
      case ErrorCode::FlagTakesNoValue:
        fmt::format_to(out, "flag '{}' takes no value", err.context);
        break;
      case ErrorCode::MissingRequiredArgument:
        fmt::format_to(out, "the required argument '{}' was not provided",
                       err.context);
        break;
      case ErrorCode::DuplicateOption:
        fmt::format_to(out, "the argument '{}' was provided more than once",
                       err.context);
        break;
      case ErrorCode::InvalidChoice:
        fmt::format_to(out, "invalid choice for argument '{}'", err.context);
        break;
      default:
        fmt::format_to(out, "unknown error occurred");
        break;

        fmt::format_to(out, "\n");
    }

    // Hint
    fmt::format_to(out, "\nFor more information, try '{}{} --help{}'.\n",
                   command_name, s(base::kBrightCyan, color_mode),
                   s(base::kReset, color_mode));
  }
  return formatted_str_;
}

}  // namespace arg
