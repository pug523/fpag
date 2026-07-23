// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/parser.h"

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
#include "fpag/base/numeric.h"

namespace arg {

namespace {

// Pure zero-allocation adapter functions
std::string_view get_from_argv(const void* ctx, usize index) {
  const char* const* argv = static_cast<const char* const*>(ctx);
  const char* str = argv[index];
  return str ? std::string_view(str) : std::string_view();
}

std::string_view get_from_span(const void* ctx, usize index) {
  auto* span = static_cast<const std::span<const std::string_view>*>(ctx);
  return (*span)[index];
}

}  // namespace

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

void Parser::add_error(ErrorCode code,
                       std::string&& context_arg,
                       std::string&& value_arg) {
  errors_.emplace_back(code, std::move(context_arg), std::move(value_arg));
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

// --key=value or --key value
bool Parser::long_option(std::string_view current,
                         usize* i,
                         const ArgSequence& args,
                         Matches* matches,
                         ParseStatus* status) {
  if (builtin_enabled_) {
    if (current == kBuiltinHelpArgLong) {
      *status = ParseStatus::HelpRequested;
      return true;
    } else if (current == kBuiltinVersionArgLong) {
      *status = ParseStatus::VersionRequested;
      return true;
    }
  }

  auto equal_pos = current.find('=');
  std::string_view key = current.substr(0, equal_pos);
  const Arg* arg = find_arg_by_long(key);

  if (!arg) {
    add_error(ErrorCode::UnknownLongOption, std::string(key));
    return false;
  }

  std::string_view value;
  if (arg->is_flag()) {
    if (equal_pos != std::string_view::npos) {
      add_error(ErrorCode::FlagTakesNoValue, fmt::format("--{}", key));
      return false;
    }
    value = "1";
  } else {
    if (equal_pos != std::string_view::npos) {
      value = current.substr(equal_pos + 1);
    } else {
      if (*i + 1 >= args.size || args[*i + 1].empty()) {
        add_error(ErrorCode::MissingValueForOption, fmt::format("--{}", key));
        return false;
      }
      value = args[++*i];
    }
  }

  if (!is_valid_choice(*arg, value)) {
    add_error(ErrorCode::InvalidChoice, std::string(value));
    return false;
  }
  matches->add(arg->name(), value);
  return false;
}

// -abc (supports chaining)
bool Parser::short_options(std::string_view current,
                           usize* i,
                           const ArgSequence& args,
                           Matches* matches,
                           ParseStatus* status) {
  current.remove_prefix(1);  // remove '-'

  for (usize c_idx = 0; c_idx < current.size(); ++c_idx) {
    char c = current[c_idx];
    const Arg* arg = find_arg_by_short(c);

    if (!arg) {
      if (builtin_enabled_) {
        if (c == kBuiltinHelpArgShort) {
          *status = ParseStatus::HelpRequested;
          return true;
        } else if (c == kBuiltinVersionArgShort) {
          *status = ParseStatus::VersionRequested;
          return true;
        }
      }
      add_error(ErrorCode::UnknownShortOption, std::string(&c, 1));
      return false;
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
        // Value is next in argument sequence
        if (*i + 1 >= args.size || args[*i + 1].empty()) {
          add_error(ErrorCode::MissingValueForOption, fmt::format("-{}", c));
          return false;
        }
        value = args[++*i];
      }
    }

    if (!is_valid_choice(*arg, value)) {
      add_error(ErrorCode::InvalidChoice, std::string(value));
      return false;
    }
    matches->add(arg->name(), value);
  }
  return false;
}

ParseStatus Parser::parse(i32 argc, const char* const* argv, Matches* matches) {
  if (argc <= 0) {
    add_error(ErrorCode::InvalidArgCount, std::to_string(argc));
    return ParseStatus::Error;
  }
  const ArgSequence seq{
      .size = static_cast<usize>(argc),
      .at = get_from_argv,
      .ctx = argv,
  };
  return parse_impl(seq, matches);
}

ParseStatus Parser::parse(std::span<const std::string_view> args,
                          Matches* matches) {
  const ArgSequence seq{
      .size = args.size(),
      .at = get_from_span,
      .ctx = &args,
  };
  return parse_impl(seq, matches);
}

ParseStatus Parser::parse_impl(const ArgSequence& args, Matches* matches) {
  if (!matches) {
    add_error(ErrorCode::NullMatchesPointer);
    return ParseStatus::Error;
  }

  // Clear previous errors for re-parsing
  errors_.clear();

  bool stop_parsing_flags = false;
  for (usize i = 1; i < args.size; ++i) {
    const std::string_view current = args[i];
    if (current.empty()) {
      continue;
    }

    if (stop_parsing_flags) {
      matches->add_positional(current);
      continue;
    }

    if (current == "--") {
      stop_parsing_flags = true;
      continue;
    }

    ParseStatus status = ParseStatus::Success;
    bool should_stop = false;
    if (current.starts_with("--")) {
      should_stop = long_option(current.substr(2), &i, args, matches, &status);
    } else if (current.starts_with("-") && current.size() > 1) {
      should_stop = short_options(current, &i, args, matches, &status);
    } else {
      matches->add_positional(current);
      continue;
    }

    if (should_stop) {
      return status;
    }
  }

  // Validate required arguments
  for (const auto& arg : args_) {
    if (arg.is_required() && !matches->has(arg.name())) {
      std::string arg_name =
          !arg.long_name().empty()
              ? fmt::format("--{}", arg.long_name())
              : fmt::format("-{}", arg.short_name().value_or('?'));

      add_error(ErrorCode::MissingRequiredArgument, std::move(arg_name));
    }
  }

  if (errors_.empty()) {
    return ParseStatus::Success;
  } else {
    return ParseStatus::Error;
  }
}

}  // namespace arg
