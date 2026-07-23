// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/base/result.h"

namespace arg {

struct ParseError {
  enum class Kind : u8 {
    HelpRequested,
    VersionRequested,
    Error,
  };

  Kind kind;
  std::string_view message;

  inline bool is_help() const { return kind == Kind::HelpRequested; }
  inline bool is_version() const { return kind == Kind::VersionRequested; }
};

template <typename Class>
using ParseResult = base::Result<Class, ParseError>;

}  // namespace arg
