// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/command.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "fmt/core.h"
#include "fmt/format.h"
#include "fpag/arg/arg.h"
#include "fpag/arg/error_code.h"
#include "fpag/arg/matches.h"
#include "fpag/arg/parse_status.h"
#include "fpag/arg/parser.h"
#include "fpag/base/numeric.h"

namespace arg {

const Arg* Command::find_arg_by_short(char c) const {
  for (const auto& arg : state_.args) {
    if (arg.short_name() && *arg.short_name() == c) {
      return &arg;
    }
  }
  return nullptr;
}

const Arg* Command::find_arg_by_long(std::string_view long_name) const {
  for (const auto& arg : state_.args) {
    if (arg.long_name() == long_name) {
      return &arg;
    }
  }
  return nullptr;
}

const Command* Command::find_subcommand(std::string_view sub_name) const {
  for (const auto& sub : state_.subcommands) {
    if (sub.name() == sub_name) {
      return &sub;
    }
  }
  return nullptr;
}

}  // namespace arg

