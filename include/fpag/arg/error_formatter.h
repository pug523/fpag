// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>

#include "fpag/arg/error_code.h"
#include "fpag/base/color_mode.h"

namespace arg {

class Command;

class ErrorFormatter {
 public:
  ErrorFormatter() = default;
  ~ErrorFormatter() = default;

  ErrorFormatter(const ErrorFormatter&) = delete;
  ErrorFormatter& operator=(const ErrorFormatter&) = delete;

  ErrorFormatter(ErrorFormatter&&) noexcept = default;
  ErrorFormatter& operator=(ErrorFormatter&&) noexcept = default;

  std::string_view format(ErrorCode code,
                          std::string_view context_arg,
                          std::string_view command_name,
                          base::ColorMode color_mode);

 private:
  std::string formatted_str_;
};

}  // namespace arg
