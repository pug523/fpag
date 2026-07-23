// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fpag/arg/arg.h"
#include "fpag/arg/error_code.h"
#include "fpag/arg/error_formatter.h"
#include "fpag/arg/help_formatter.h"
#include "fpag/arg/matches.h"
#include "fpag/arg/parse_error.h"
#include "fpag/arg/parse_status.h"
#include "fpag/base/color_mode.h"
#include "fpag/base/console.h"
#include "fpag/base/numeric.h"

namespace arg {

/// Command-line argument parser that processes command-line input and
/// populates argument matches.
class Parser {
 public:
  /// @param name Program name displayed in help and error output.
  /// @param version Version string displayed when `--version` or `-v' passed.
  /// @param builtin_enabled Automatically registers help and version flags.
  /// @param color_mode Controls ANSI color formatting.
  Parser(std::string_view name,
         std::string_view version,
         bool builtin_enabled = true,
         base::ColorMode color_mode =
             base::console_color_mode(base::Stream::Stdout))
      : name_(name),
        version_(version),
        builtin_enabled_(builtin_enabled),
        color_mode_(color_mode) {}
  ~Parser() = default;

  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  Parser(Parser&&) noexcept = default;
  Parser& operator=(Parser&&) noexcept = default;

  /// Sets the top-level description of the application for help output.
  inline Parser& about(std::string_view description) & {
    about_ = description;
    return *this;
  }

  inline Parser&& about(std::string_view description) && {
    about_ = description;
    return std::move(*this);
  }

  /// Registers a command-line argument definition with the parser.
  inline Parser& add_arg(Arg&& arg) & {
    args_.push_back(std::move(arg));
    return *this;
  }

  inline Parser&& add_arg(Arg&& arg) && {
    args_.push_back(std::move(arg));
    return std::move(*this);
  }

  /// Zero-copy C-style argc/argv parser.
  ///
  /// @param argc Number of arguments in @p argv.
  /// @param argv Array of argument strings (typically passed from main).
  /// @param[out] matches Destination object to store parsed argument values.
  /// @return Status indicating success, error, help/version requested
  ParseStatus parse(i32 argc, const char* const* argv, Matches* matches);

  /// Zero-copy std::span parser. Accepts contiguous sequences like
  /// std::vector<std::string_view> directly.
  ///
  /// @param args Sequence of argument views to parse.
  /// @param[out] matches Destination object to store parsed argument values.
  /// @return Status indicating success, error, help/version requested
  ParseStatus parse(std::span<const std::string_view> args, Matches* matches);

  /// Overload for fixed-size array literals.
  template <usize N>
  inline ParseStatus parse(const std::string_view (&args)[N],
                           Matches* matches) {
    return parse(std::span<const std::string_view>(args, N), matches);
  }

  inline std::string_view name() const { return name_; }
  inline std::string_view version() const { return version_; }
  inline std::string_view about() const { return about_; }
  inline const std::vector<Arg>& args() const& { return args_; }
  inline const std::vector<ParseError>& errors() const& { return errors_; }

  inline bool builtin_enabled() const { return builtin_enabled_; }
  inline base::ColorMode color_mode() const { return color_mode_; }

  inline std::string_view error_message() & {
    return error_formatter_.format(errors_, name_, color_mode_);
  }
  inline std::string_view help_message() & {
    return help_formatter_.format(*this, color_mode_);
  }

  inline std::vector<Arg>&& args() && { return std::move(args_); }
  inline std::vector<ParseError>&& errors() && { return std::move(errors_); }
  inline std::string&& error_message() && {
    return std::move(error_formatter_).format(errors_, name_, color_mode_);
  }
  inline std::string&& help_message() && {
    return std::move(help_formatter_).format(*this, color_mode_);
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

  ParseStatus parse_impl(const ArgSequence& args, Matches* matches);

  const Arg* find_arg_by_short(char c) const;
  const Arg* find_arg_by_long(std::string_view name) const;

  void add_error(ErrorCode code,
                 std::string&& context_arg = "",
                 std::string&& value_arg = "");

  // Returns true if should stop parsing.
  bool long_option(std::string_view current,
                   usize* i,
                   const ArgSequence& args,
                   Matches* matches,
                   ParseStatus* status);

  // Returns true if should stop parsing.
  bool short_options(std::string_view current,
                     usize* i,
                     const ArgSequence& args,
                     Matches* matches,
                     ParseStatus* status);
  bool is_valid_choice(const Arg& arg, std::string_view value) const;

  std::string_view name_;
  std::string_view version_;
  std::string_view about_;
  std::vector<Arg> args_;

  std::vector<ParseError> errors_;

  ErrorFormatter error_formatter_;
  HelpFormatter help_formatter_;
  bool builtin_enabled_;
  base::ColorMode color_mode_;
};

}  // namespace arg
