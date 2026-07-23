// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <utility>

#include "fpag/arg/error_code.h"

namespace arg {

struct ParseError {
  ErrorCode code{ErrorCode::None};
  /// Flag/option name (e.g., "--foo" or "-f")
  std::string context;
  /// Optional context value (e.g., invalid choice supplied)
  std::string value;

  ParseError(ErrorCode c, std::string ctx, std::string val = "")
      : code(c), context(std::move(ctx)), value(std::move(val)) {}
};

}  // namespace arg
