// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fpag/arg/arg.h"
#include "fpag/arg/command.h"
#include "fpag/arg/error_code.h"
#include "fpag/arg/error_formatter.h"
#include "fpag/arg/help_formatter.h"
#include "fpag/arg/matches.h"
#include "fpag/arg/parse_error.h"
#include "fpag/arg/parse_result.h"
#include "fpag/arg/parse_status.h"
#include "fpag/base/color_style.h"
#include "fpag/base/console.h"
#include "fpag/base/numeric.h"

namespace arg {

/// Stateful execution engine for parsing command-line input against a Command
/// schema.
class Parser {
 public:
  explicit Parser(Command&& root_command)
      : root_cmd_(std::move(root_command)) {}
  ~Parser() = default;

  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  Parser(Parser&&) noexcept = default;
  Parser& operator=(Parser&&) noexcept = default;

  /// Full zero-allocation parsers
  ParseStatus parse(i32 argc, const char* const* argv, Matches* matches);
  ParseStatus parse(std::span<const std::string_view> args, Matches* matches);

  /// Partial zero-allocation parsers
  /// Stores unrecognized options and positional arguments into @p unparsed.
  ParseStatus parse_partial(i32 argc,
                            const char* const* argv,
                            Matches* matches,
                            std::vector<std::string_view>* unparsed);
  ParseStatus parse_partial(std::span<const std::string_view> args,
                            Matches* matches,
                            std::vector<std::string_view>* unparsed);

  ParseResult<Matches> try_parse(i32 argc, const char* const* argv) &;
  ParseResult<Matches> try_parse(std::span<const std::string_view> args) &;

  ParseResult<Matches> try_parse(i32 argc, const char* const* argv) &&;
  ParseResult<Matches> try_parse(std::span<const std::string_view> args) &&;

  /// Fixed-size array overloads
  template <usize N>
  inline ParseStatus parse(const std::string_view (&args)[N],
                           Matches* matches) {
    return parse(std::span<const std::string_view>(args, N), matches);
  }

  template <usize N>
  inline ParseStatus parse_partial(const std::string_view (&args)[N],
                                   Matches* matches,
                                   std::vector<std::string_view>* unparsed) {
    return parse_partial(std::span<const std::string_view>(args, N), matches,
                         unparsed);
  }

  inline const Command& root_command() const { return root_cmd_; }

  inline const std::vector<ParseError>& errors() const& { return errors_; }
  inline std::vector<ParseError>&& errors() && { return std::move(errors_); }

  inline std::string_view error_message(
      base::ColorStyle color_style =
          base::console_color_style(base::Stream::Stdout)) & {
    return error_formatter_.format(errors_, root_cmd_.name(), color_style);
  }

  inline std::string_view help_message(
      base::ColorStyle color_style =
          base::console_color_style(base::Stream::Stdout)) & {
    return help_formatter_.format(root_cmd_, color_style);
  }

  inline std::string&& error_message(
      base::ColorStyle color_style =
          base::console_color_style(base::Stream::Stdout)) && {
    return std::move(error_formatter_)
        .format(errors_, root_cmd_.name(), color_style);
  }

  inline std::string&& help_message(
      base::ColorStyle color_style =
          base::console_color_style(base::Stream::Stdout)) && {
    return std::move(help_formatter_).format(root_cmd_, color_style);
  }

  static constexpr const char* kBuiltinHelpArgLong = "help";
  static constexpr const char kBuiltinHelpArgShort = 'h';
  static constexpr const char* kBuiltinVersionArgLong = "version";
  static constexpr const char kBuiltinVersionArgShort = 'v';

 private:
  struct ArgSequence {
    usize size;
    std::string_view (*at)(const void* ctx, usize index);
    const void* ctx;

    inline std::string_view operator[](usize index) const {
      return at(ctx, index);
    }
  };

  // Internal state container for unified parsing logic
  struct ParseContext {
    const ArgSequence& args;
    Matches* matches;
    std::vector<std::string_view>* unparsed = nullptr;
    bool partial_mode = false;
  };

  ParseStatus parse_impl(ParseContext& ctx);

  void add_error(ErrorCode code,
                 std::string&& context_arg = "",
                 std::string&& value_arg = "");

  bool long_option(const Command& cmd,
                   std::string_view raw_arg,
                   usize* i,
                   ParseContext& ctx,
                   ParseStatus* status);

  bool short_options(const Command& cmd,
                     std::string_view raw_arg,
                     usize* i,
                     ParseContext& ctx,
                     ParseStatus* status);

  bool is_valid_choice(const Arg& arg, std::string_view value) const;

  Command root_cmd_;
  std::vector<ParseError> errors_;

  ErrorFormatter error_formatter_;
  HelpFormatter help_formatter_;
};

}  // namespace arg
