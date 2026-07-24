// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace arg {

enum class ErrorCode : u8 {
  None = 0,
  InvalidArgCount,
  NullMatchesPointer,
  UnknownLongOption,
  UnknownShortOption,
  MissingValueForOption,
  FlagTakesNoValue,
  MissingRequiredArgument,
  DuplicateOption,
  InvalidChoice,
};

constexpr const char* ec_to_format_str(ErrorCode error_code) {
  switch (error_code) {
    case ErrorCode::InvalidArgCount: return "invalid arg count provided: {}";
    case ErrorCode::NullMatchesPointer: return "output matches pointer is null";
    case ErrorCode::UnknownLongOption:
      return "unexpected argument '--{}' found";
    case ErrorCode::UnknownShortOption:
      return "unexpected argument '-{}' found";
    case ErrorCode::MissingValueForOption:
      return "a value is required for '{}' but none was supplied";
    case ErrorCode::FlagTakesNoValue: return "flag '{}' takes no value";
    case ErrorCode::MissingRequiredArgument:
      return "the required argument '{}' was not provided";
    case ErrorCode::DuplicateOption:
      return "the argument '{}' was provided more than once";
    case ErrorCode::InvalidChoice:
      return "invalid choice for argument '{}': '{}'";
    default: return "unknown error occurred";
  }
}

}  // namespace arg
