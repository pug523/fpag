// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/error_formatter.h"

#include <iterator>
#include <string_view>

#include "fmt/base.h"
#include "fmt/core.h"
#include "fpag/arg/error_code.h"
#include "fpag/base/color_mode.h"
#include "fpag/base/numeric.h"
#include "fpag/base/style.h"

namespace arg {

namespace {

inline const char* s(const char* code, base::ColorMode mode) noexcept {
  return base::style_code(code, mode);
}

}  // namespace

std::string_view ErrorFormatter::format(ErrorCode code,
                                        std::string_view context_arg,
                                        std::string_view command_name,
                                        base::ColorMode color_mode) {
  formatted_str_.clear();
  constexpr usize kEstimatedErrorStrLen = 256;
  formatted_str_.reserve(kEstimatedErrorStrLen);

  auto out = std::back_inserter(formatted_str_);

  // "error: " header
  fmt::format_to(out, "{}{}{} ", s(base::kBrightRed, color_mode),
                 s(base::kBold, color_mode), "error:");

  // Detail error message
  switch (code) {
    case ErrorCode::NullMatchesPointer:
      fmt::format_to(out, "output matches pointer is null");
      break;
    case ErrorCode::UnknownLongOption:
      fmt::format_to(out, "unexpected argument '--{}' found", context_arg);
      break;
    case ErrorCode::UnknownShortOption:
      fmt::format_to(out, "unexpected argument '-{}' found", context_arg);
      break;
    case ErrorCode::MissingValueForOption:
      fmt::format_to(out, "a value is required for '{}' but none was supplied",
                     context_arg);
      break;
    case ErrorCode::FlagTakesNoValue:
      fmt::format_to(out, "flag '{}' takes no value", context_arg);
      break;
    case ErrorCode::MissingRequiredArgument:
      fmt::format_to(out, "the required argument '{}' was not provided",
                     context_arg);
      break;
    case ErrorCode::DuplicateOption:
      fmt::format_to(out, "the argument '{}' was provided more than once",
                     context_arg);
      break;
    default: fmt::format_to(out, "unknown error occurred"); break;
  }

  // Help hint
  fmt::format_to(out, "{}\n\nFor more information, try '{}{} --help{}'.\n",
                 s(base::kReset, color_mode), command_name,
                 s(base::kBrightCyan, color_mode), s(base::kReset, color_mode));

  return formatted_str_;
}

}  // namespace arg
