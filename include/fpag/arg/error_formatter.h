// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fpag/arg/parse_error.h"
#include "fpag/base/color_mode.h"

namespace arg {

class ErrorFormatter {
 public:
  ErrorFormatter() = default;
  ~ErrorFormatter() = default;

  ErrorFormatter(const ErrorFormatter&) = delete;
  ErrorFormatter& operator=(const ErrorFormatter&) = delete;

  ErrorFormatter(ErrorFormatter&&) noexcept = default;
  ErrorFormatter& operator=(ErrorFormatter&&) noexcept = default;

  std::string_view format(const std::vector<ParseError>& errors,
                          std::string_view command_name,
                          base::ColorMode color_mode) &;

  inline std::string&& format(const std::vector<ParseError>& errors,
                              std::string_view command_name,
                              base::ColorMode color_mode) && {
    format(errors, command_name, color_mode);
    return std::move(formatted_str_);
  }

 private:
  std::string formatted_str_;
};

}  // namespace arg
