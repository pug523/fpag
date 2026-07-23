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

class Parser {
 public:
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

  inline Parser& about(std::string_view description) & {
    about_ = description;
    return *this;
  }

  inline Parser&& about(std::string_view description) && {
    about_ = description;
    return std::move(*this);
  }

  inline Parser& add_arg(Arg&& arg) & {
    args_.push_back(std::move(arg));
    return *this;
  }

  inline Parser&& add_arg(Arg&& arg) && {
    args_.push_back(std::move(arg));
    return std::move(*this);
  }

  // Zero-copy C-style argc/argv parser
  ParseStatus parse(i32 argc, const char* const* argv, Matches* matches);

  // Zero-copy std::span parser
  // (accepts std::vector<std::string_view> automatically)
  ParseStatus parse(std::span<const std::string_view> args, Matches* matches);

  // Overload for array literals
  template <size_t N>
  inline ParseStatus parse(const std::string_view (&args)[N],
                           Matches* matches) {
    return parse(std::span<const std::string_view>(args, N), matches);
  }

  inline std::string_view name() const { return name_; }
  inline std::string_view version() const { return version_; }
  inline std::string_view about() const { return about_; }
  inline const std::vector<Arg>& args() const { return args_; }
  inline const std::vector<ParseError>& errors() const& { return errors_; }
  inline std::vector<ParseError>&& errors() && { return std::move(errors_); }
  inline bool builtin_enabled() const { return builtin_enabled_; }
  inline base::ColorMode color_mode() const { return color_mode_; }

  inline std::string_view error_message() {
    return error_formatter_.format(errors_, name_, color_mode_);
  }
  inline std::string_view help_message() {
    return help_formatter_.format(*this, color_mode_);
  }

  static constexpr const char* kBuiltinHelpArgLong = "help";
  static constexpr const char kBuiltinHelpArgShort = 'h';
  static constexpr const char* kBuiltinVersionArgLong = "version";
  static constexpr const char kBuiltinVersionArgShort = 'v';

 private:
  struct ArgSequence {
    size_t size;
    std::string_view (*at)(const void* ctx, size_t index);
    const void* ctx;

    inline std::string_view operator[](size_t index) const {
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
