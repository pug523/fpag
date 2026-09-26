// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "fpag/arg/arg.h"
#include "fpag/arg/command.h"
#include "fpag/arg/matches.h"
#include "fpag/arg/parse_result.h"
#include "fpag/arg/parser.h"
#include "fpag/base/numeric.h"
#include "fpag/debug/fatal.h"

namespace arg::detail {

// A binder object that knows how to map an arg::Arg to a struct member.
template <typename Class, typename T>
struct ArgBinder {
  Arg arg;
  T Class::* member;

  // Registers the argument to the CommandBuilder.
  inline void apply_to_builder(CommandBuilder* builder) && {
    // SAFETY: This is assumed to be called only by macros, and they have
    // builder instance so its pointer can not be null.
    builder->add_arg(std::move(arg));
  }

  // Extracts the parsed value from Matches into the struct member. Returns
  // false and fills @p out_error when a matched value cannot be converted to
  // the member type, so the caller reports it instead of leaving the member
  // at its default.
  inline bool extract(Class* obj,
                      const Matches* matches,
                      ParseError* out_error) const {
    // SAFETY: This is assumed to be called only by macros, and they have
    // both obj and matches instance so their pointers can not be null.
    if (arg.is_flag()) {
      if constexpr (std::is_same_v<T, bool>) {
        obj->*member = matches->has(arg.name());
      }
      return true;
    }

    if (!matches->has(arg.name())) {
      return true;
    }

    auto res = matches->get<T>(arg.name());
    if (!res.is_ok()) {
      const std::string_view value = matches->get<std::string_view>(arg.name())
                                         .unwrap_or(std::string_view{});
      *out_error = ParseError(ErrorCode::InvalidValue, std::string(arg.name()),
                              std::string(value));
      return false;
    }

    obj->*member = std::move(res).unwrap();
    return true;
  }
};

template <typename Class, typename... Binders>
ParseResult<Class> parse_macro_impl(i32 argc,
                                    const char* const* argv,
                                    CommandBuilder&& builder,
                                    Binders&&... binders) {
  (std::forward<Binders>(binders).apply_to_builder(&builder), ...);
  Parser parser(std::move(builder).build());
  ParseResult<Matches> result = parser.try_parse(argc, argv);
  if (result.is_ok()) {
    Class config{};
    Matches matches = std::move(result).unwrap();

    // The fold short-circuits on the first binder whose value cannot be
    // converted, and that binder fills in the error reported below.
    ParseError error{ErrorCode::None, ""};
    const bool converted = (binders.extract(&config, &matches, &error) && ...);
    if (!converted) {
      return ParseResult<Class>::make_err(std::vector<ParseError>{error});
    }

    return ParseResult<Class>::make_ok(std::move(config));
  } else if (result.is_err()) {
    return ParseResult<Class>::make_err(std::move(result).unwrap_err());
  } else if (result.is_help()) {
    return ParseResult<Class>::make_help(std::move(result).unwrap_help());
  } else if (result.is_version()) {
    return ParseResult<Class>::make_version(std::move(result).unwrap_version());
  }
  FPAG_UNREACHABLE_MSG(
      "parse result is neither ok nor err nor help nor version");
}

}  // namespace arg::detail

#define ARGS_OPT_FULL(Class, Field, Short, Long, Default, ValueName, Help, \
                      Required, ...)                                       \
  ::arg::detail::ArgBinder<Class, decltype(Class::Field)> {                \
    std::move(::arg::ArgBuilder(Long)                                      \
                  .short_name(Short)                                       \
                  .name(#Field)                                            \
                  .default_value(Default)                                  \
                  .value_name(ValueName)                                   \
                  .help(Help)                                              \
                  .required(Required)                                      \
                  .choices(__VA_OPT__({__VA_ARGS__})))                     \
        .build(),                                                          \
        &Class::Field,                                                     \
  }

#define ARGS_OPT(Class, Field, Short, Long, Help, Required, ...) \
  ::arg::detail::ArgBinder<Class, decltype(Class::Field)> {      \
    std::move(::arg::ArgBuilder(Long)                            \
                  .short_name(Short)                             \
                  .name(#Field)                                  \
                  .help(Help)                                    \
                  .required(Required)                            \
                  .choices(__VA_OPT__({__VA_ARGS__})))           \
        .build(),                                                \
        &Class::Field,                                           \
  }

// Defines a boolean flag mapping.
#define ARGS_FLAG(Class, Field, Short, Long, Help) \
  ::arg::detail::ArgBinder<Class, bool> {          \
    std::move(::arg::ArgBuilder(Long)              \
                  .short_name(Short)               \
                  .name(#Field)                    \
                  .help(Help)                      \
                  .is_flag(true))                  \
        .build(),                                  \
        &Class::Field,                             \
  }

/// Generates a Standalone CommandBuilder with basic metadata.
#define CREATE_PARSER(CommandName, Version, About) \
  ::arg::CommandBuilder(CommandName, Version).about(About)

// Generates a parse function for a user-defined struct using CommandBuilder.
#define ARGS_FN_DEFINE(Class, FnName, CommandName, Version, About, ...)        \
  inline ::arg::ParseResult<Class> FnName(i32 argc, const char* const* argv) { \
    return ::arg::detail::parse_macro_impl<Class>(                             \
        argc, argv, CREATE_PARSER(CommandName, Version, About), __VA_ARGS__);  \
  }
