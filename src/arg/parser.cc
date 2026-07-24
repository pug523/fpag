// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/parser.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fmt/core.h"
#include "fmt/format.h"
#include "fpag/arg/arg.h"
#include "fpag/arg/command.h"
#include "fpag/arg/error_code.h"
#include "fpag/arg/matches.h"
#include "fpag/arg/parse_status.h"
#include "fpag/base/numeric.h"

namespace arg {

namespace {

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

bool Parser::long_option(const Command& cmd,
                         std::string_view raw_arg,
                         usize* i,
                         ParseContext& ctx,
                         ParseStatus* status) {
  const std::string_view content = raw_arg.substr(2);  // Strip "--"

  if (cmd.builtin_enabled()) {
    if (content == kBuiltinHelpArgLong) {
      *status = ParseStatus::HelpRequested;
      return true;
    } else if (content == kBuiltinVersionArgLong) {
      *status = ParseStatus::VersionRequested;
      return true;
    }
  }

  auto equal_pos = content.find('=');
  std::string_view key = content.substr(0, equal_pos);
  const Arg* arg = cmd.find_arg_by_long(key);

  if (!arg) {
    if (ctx.partial_mode) {
      if (ctx.unparsed) {
        ctx.unparsed->push_back(raw_arg);
      }
      return false;
    }
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
      value = content.substr(equal_pos + 1);
    } else {
      if (*i + 1 >= ctx.args.size || ctx.args[*i + 1].empty()) {
        add_error(ErrorCode::MissingValueForOption, fmt::format("--{}", key));
        return false;
      }
      value = ctx.args[++*i];
    }
  }

  if (!is_valid_choice(*arg, value)) {
    add_error(ErrorCode::InvalidChoice, std::string(value));
    return false;
  }
  ctx.matches->add(arg->name(), value);
  return false;
}

bool Parser::short_options(const Command& cmd,
                           std::string_view raw_arg,
                           usize* i,
                           ParseContext& ctx,
                           ParseStatus* status) {
  const std::string_view current = raw_arg.substr(1);  // Strip '-'

  for (usize c_idx = 0; c_idx < current.size(); ++c_idx) {
    char c = current[c_idx];
    const Arg* arg = cmd.find_arg_by_short(c);

    if (!arg) {
      if (cmd.builtin_enabled()) {
        if (c == kBuiltinHelpArgShort) {
          *status = ParseStatus::HelpRequested;
          return true;
        } else if (c == kBuiltinVersionArgShort) {
          *status = ParseStatus::VersionRequested;
          return true;
        }
      }

      if (ctx.partial_mode) {
        if (ctx.unparsed) {
          // Keep raw argument if unrecognized
          ctx.unparsed->push_back(raw_arg);
        }
        return false;
      }

      add_error(ErrorCode::UnknownShortOption, std::string(&c, 1));
      return false;
    }

    std::string_view value;
    if (arg->is_flag()) {
      value = "1";
    } else {
      if (c_idx + 1 < current.size()) {
        value = current.substr(c_idx + 1);
        c_idx = current.size();  // Consume rest as value
      } else {
        if (*i + 1 >= ctx.args.size || ctx.args[*i + 1].empty()) {
          add_error(ErrorCode::MissingValueForOption, fmt::format("-{}", c));
          return false;
        }
        value = ctx.args[++*i];
      }
    }

    if (!is_valid_choice(*arg, value)) {
      add_error(ErrorCode::InvalidChoice, std::string(value));
      return false;
    }
    ctx.matches->add(arg->name(), value);
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
  ParseContext ctx{
      .args = seq,
      .matches = matches,
      .partial_mode = false,
  };
  return parse_impl(ctx);
}

ParseStatus Parser::parse(std::span<const std::string_view> args,
                          Matches* matches) {
  const ArgSequence seq{
      .size = args.size(),
      .at = get_from_span,
      .ctx = &args,
  };
  ParseContext ctx{
      .args = seq,
      .matches = matches,
      .partial_mode = false,
  };
  return parse_impl(ctx);
}

ParseStatus Parser::parse_partial(i32 argc,
                                  const char* const* argv,
                                  Matches* matches,
                                  std::vector<std::string_view>* unparsed) {
  if (argc <= 0) {
    add_error(ErrorCode::InvalidArgCount, std::to_string(argc));
    return ParseStatus::Error;
  }
  const ArgSequence seq{
      .size = static_cast<usize>(argc),
      .at = get_from_argv,
      .ctx = argv,
  };
  ParseContext ctx{
      .args = seq,
      .matches = matches,
      .unparsed = unparsed,
      .partial_mode = true,
  };
  return parse_impl(ctx);
}

ParseStatus Parser::parse_partial(std::span<const std::string_view> args,
                                  Matches* matches,
                                  std::vector<std::string_view>* unparsed) {
  const ArgSequence seq{
      .size = args.size(),
      .at = get_from_span,
      .ctx = &args,
  };
  ParseContext ctx{
      .args = seq,
      .matches = matches,
      .unparsed = unparsed,
      .partial_mode = true,
  };
  return parse_impl(ctx);
}

ParseStatus Parser::parse_impl(ParseContext& ctx) {
  if (!ctx.matches) {
    add_error(ErrorCode::NullMatchesPointer);
    return ParseStatus::Error;
  }

  errors_.clear();
  bool stop_parsing_flags = false;

  for (usize i = 1; i < ctx.args.size; ++i) {
    const std::string_view current = ctx.args[i];
    if (current.empty()) {
      continue;
    }

    if (stop_parsing_flags) {
      if (ctx.partial_mode) {
        if (ctx.unparsed) {
          ctx.unparsed->push_back(current);
        }
      } else {
        ctx.matches->add_positional(current);
      }
      continue;
    }

    if (current == "--") {
      stop_parsing_flags = true;
      if (ctx.partial_mode && ctx.unparsed) {
        ctx.unparsed->push_back(current);
      }
      continue;
    }

    ParseStatus status = ParseStatus::Success;
    bool should_stop = false;

    if (current.starts_with("--")) {
      should_stop = long_option(root_cmd_, current, &i, ctx, &status);
    } else if (current.starts_with("-") && current.size() > 1) {
      should_stop = short_options(root_cmd_, current, &i, ctx, &status);
    } else {
      if (ctx.partial_mode) {
        if (ctx.unparsed) {
          ctx.unparsed->push_back(current);
        }
      } else {
        ctx.matches->add_positional(current);
      }
      continue;
    }

    if (should_stop) {
      return status;
    }
  }

  // Validate required arguments for root_cmd
  for (const auto& arg : root_cmd_.args()) {
    if (arg.is_required() && !ctx.matches->has(arg.name())) {
      std::string arg_name =
          !arg.long_name().empty()
              ? fmt::format("--{}", arg.long_name())
              : fmt::format("-{}", arg.short_name().value_or('?'));

      add_error(ErrorCode::MissingRequiredArgument, std::move(arg_name));
    }
  }

  return errors_.empty() ? ParseStatus::Success : ParseStatus::Error;
}

}  // namespace arg
