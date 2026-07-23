// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/command.h"

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

const Arg* Command::find_arg_by_short(char c) const {
  for (const auto& arg : args_) {
    if (arg.short_name() && *arg.short_name() == c) {
      return &arg;
    }
  }
  return nullptr;
}

const Arg* Command::find_arg_by_long(std::string_view name) const {
  for (const auto& arg : args_) {
    if (arg.long_name() == name) {
      return &arg;
    }
  }
  return nullptr;
}

ParseStatus Command::set_error(ErrorCode code, std::string_view context_arg) {
  last_error_code_ = code;
  error_context_arg_ = context_arg;
  return ParseStatus::Error;
}

ParseStatus Command::parse(i32 argc,
                           const char* const* argv,
                           Matches* matches) {
  if (matches == nullptr) {
    return set_error(ErrorCode::NullMatchesPointer);
  }

  last_error_code_ = ErrorCode::None;
  error_context_arg_.clear();

  if (argc <= 1 || argv == nullptr) {
    return ParseStatus::Success;
  }

  bool stop_parsing_flags = false;

  for (i32 i = 1; i < argc; ++i) {
    if (argv[i] == nullptr) {
      continue;
    }
    std::string_view current(argv[i]);

    if (stop_parsing_flags) {
      matches->add_positional(current);
      continue;
    }

    // "--" Positional separator
    if (current == "--") {
      stop_parsing_flags = true;
      continue;
    }

    // Long options
    if (current.starts_with("--")) {
      current.remove_prefix(2);

      // Built-in flag checks
      if (current == "help") {
        return ParseStatus::HelpRequested;
      }
      if (current == "version" && !version_.empty()) {
        return ParseStatus::VersionRequested;
      }

      auto equal_pos = current.find('=');
      std::string_view key = current.substr(0, equal_pos);

      const Arg* arg = find_arg_by_long(key);
      if (arg == nullptr) {
        return set_error(ErrorCode::UnknownLongOption, key);
      }

      if (arg->is_flag()) {
        if (equal_pos != std::string_view::npos) {
          return set_error(ErrorCode::FlagTakesNoValue,
                           fmt::format("--{}", key));
        }
        matches->add(arg->name(), "1");
      } else {
        if (equal_pos != std::string_view::npos) {
          matches->add(arg->name(), current.substr(equal_pos + 1));
        } else {
          if (i + 1 >= argc || argv[i + 1] == nullptr) {
            return set_error(ErrorCode::MissingValueForOption,
                             fmt::format("--{}", key));
          }
          matches->add(arg->name(), argv[++i]);
        }
      }
    } else if (current.starts_with("-") && current.size() > 1) {
      // Short options
      current.remove_prefix(1);

      for (size_t c_idx = 0; c_idx < current.size(); ++c_idx) {
        char c = current[c_idx];

        // Built-in flag checks
        if (c == 'h' && current.size() == 1) {
          return ParseStatus::HelpRequested;
        }
        if (c == 'v' && current.size() == 1 && !version_.empty()) {
          return ParseStatus::VersionRequested;
        }

        const Arg* arg = find_arg_by_short(c);
        if (arg == nullptr) {
          return set_error(ErrorCode::UnknownShortOption,
                           std::string_view(&c, 1));
        }

        if (arg->is_flag()) {
          matches->add(arg->name(), "1");
        } else {
          if (c_idx + 1 < current.size()) {
            matches->add(arg->name(), current.substr(c_idx + 1));
            break;
          } else {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
              return set_error(ErrorCode::MissingValueForOption,
                               fmt::format("-{}", c));
            }
            matches->add(arg->name(), argv[++i]);
          }
        }
      }
    } else {
      // Positional arguments
      matches->add_positional(current);
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
