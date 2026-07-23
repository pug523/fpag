// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/parser.h"

#include <fmt/core.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "fmt/format.h"
#include "fpag/arg/arg.h"
#include "fpag/arg/error_code.h"
#include "fpag/arg/matches.h"
#include "fpag/base/numeric.h"

namespace arg {

const Arg* Parser::find_arg_by_short(char c) const {
  for (const auto& arg : args_) {
    if (arg.short_name() && *arg.short_name() == c) {
      return &arg;
    }
  }
  return nullptr;
}

const Arg* Parser::find_arg_by_long(std::string_view name) const {
  for (const auto& arg : args_) {
    if (arg.long_name() == name) {
      return &arg;
    }
  }
  return nullptr;
}

ParseStatus Parser::set_error(ErrorCode code, std::string_view context_arg) {
  last_error_code_ = code;
  error_context_arg_ = context_arg;
  return ParseStatus::Error;
}

bool Parser::is_valid_choice(const Arg& arg, std::string_view value) const {
  if (arg.choices().empty()) {
    return true;
  }
  for (const auto& choice : arg.choices()) {
    if (choice == value) {
      return true;
    }
  }
  return false;
}

// Logic for long options --key=value or --key value
ParseStatus Parser::long_option(std::string_view current,
                                i32* i,
                                i32 argc,
                                const char* const* argv,
                                Matches* matches) {
  if (builtin_enabled_) {
    if (current == "--help") {
      return ParseStatus::HelpRequested;
    }

    if (current == "--version") {
      return ParseStatus::VersionRequested;
    }
  }

  auto equal_pos = current.find('=');
  std::string_view key = current.substr(0, equal_pos);
  const Arg* arg = find_arg_by_long(key);

  if (!arg) {
    return set_error(ErrorCode::UnknownLongOption, key);
  }

  std::string_view value;
  if (arg->is_flag()) {
    if (equal_pos != std::string_view::npos) {
      return set_error(ErrorCode::FlagTakesNoValue, fmt::format("--{}", key));
    }
    value = "1";
  } else {
    if (equal_pos != std::string_view::npos) {
      value = current.substr(equal_pos + 1);
    } else {
      if (*i + 1 >= argc || argv[*i + 1] == nullptr) {
        return set_error(ErrorCode::MissingValueForOption,
                         fmt::format("--{}", key));
      }
      value = argv[++*i];
    }
  }

  if (!is_valid_choice(*arg, value)) {
    return set_error(ErrorCode::InvalidChoice, value);
  }
  matches->add(arg->name(), value);
  return ParseStatus::Success;
}

// Logic for short options -abc (supports chaining)
ParseStatus Parser::short_options(std::string_view current,
                                  i32* i,
                                  i32 argc,
                                  const char* const* argv,
                                  Matches* matches) {
  current.remove_prefix(1);  // remove '-'

  for (size_t c_idx = 0; c_idx < current.size(); ++c_idx) {
    char c = current[c_idx];
    const Arg* arg = find_arg_by_short(c);

    if (!arg) {
      if (builtin_enabled_) {
        if (c == 'h') {
          return ParseStatus::HelpRequested;
        }
        if (c == 'v') {
          return ParseStatus::VersionRequested;
        }
      }
      return set_error(ErrorCode::UnknownShortOption, std::string_view(&c, 1));
    }

    std::string_view value;
    if (arg->is_flag()) {
      value = "1";
    } else {
      // If value is inline, e.g., -fValue
      if (c_idx + 1 < current.size()) {
        value = current.substr(c_idx + 1);
        c_idx = current.size();  // Break loop
      } else {
        // Value is next in argv
        if (*i + 1 >= argc || argv[*i + 1] == nullptr) {
          return set_error(ErrorCode::MissingValueForOption,
                           fmt::format("-{}", c));
        }
        value = argv[++*i];
      }
    }

    if (!is_valid_choice(*arg, value)) {
      return set_error(ErrorCode::InvalidChoice, value);
    }
    matches->add(arg->name(), value);
  }
  return ParseStatus::Success;
}

ParseStatus Parser::parse(i32 argc, const char* const* argv, Matches* matches) {
  if (!matches) {
    return set_error(ErrorCode::NullMatchesPointer);
  }

  bool stop_parsing_flags = false;
  for (i32 i = 1; i < argc; ++i) {
    if (!argv[i]) {
      continue;
    }
    std::string_view current(argv[i]);

    if (stop_parsing_flags) {
      matches->add_positional(current);
      continue;
    }

    if (current == "--") {
      stop_parsing_flags = true;
      continue;
    }

    ParseStatus status = ParseStatus::Success;
    if (current.starts_with("--")) {
      status = long_option(current.substr(2), &i, argc, argv, matches);
    } else if (current.starts_with("-") && current.size() > 1) {
      status = short_options(current, &i, argc, argv, matches);
    } else {
      matches->add_positional(current);
      continue;
    }

    if (status != ParseStatus::Success) {
      return status;
    }
  }

  // Validate required arguments
  for (const auto& arg : args_) {
    if (arg.is_required() && !matches->has(arg.name())) {
      const std::string arg_name =
          !arg.long_name().empty()
              ? fmt::format("--{}", arg.long_name())
              : fmt::format("-{}", arg.short_name().value_or('?'));

      return set_error(ErrorCode::MissingRequiredArgument, arg_name);
    }
  }

  return ParseStatus::Success;
}

}  // namespace arg
