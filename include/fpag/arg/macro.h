// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <utility>

#include "fpag/arg/arg.h"
#include "fpag/arg/command.h"
#include "fpag/arg/matches.h"
#include "fpag/arg/parse_result.h"
#include "fpag/arg/parse_status.h"
#include "fpag/arg/parser.h"
#include "fpag/base/console.h"
#include "fpag/base/numeric.h"

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

  // Extracts the parsed value from Matches into the struct member.
  inline void extract(Class* obj, const Matches* matches) const {
    // SAFETY: This is assumed to be called only by macros, and they have
    // both obj and matches instance so their pointers can not be null.
    if (arg.is_flag()) {
      if constexpr (std::is_same_v<T, bool>) {
        obj->*member = matches->has(arg.name());
      }
    } else {
      if (matches->has(arg.name())) {
        auto res = matches->get<T>(arg.name());
        if (res.is_ok()) {
          obj->*member = std::move(res).unwrap();
        }
      }
    }
  }
};

template <typename Class, typename... Binders>
arg::ParseResult<Class> parse_macro_impl(i32 argc,
                                         const char* const* argv,
                                         CommandBuilder&& builder,
                                         Binders&&... binders) {
  (std::forward<Binders>(binders).apply_to_builder(&builder), ...);

  Parser parser(std::move(builder).build());

  Matches matches;
  const ParseStatus status = parser.parse(argc, argv, &matches);
  Class config{};

  switch (status) {
    case ParseStatus::Success: {
      (binders.extract(&config, &matches), ...);
      return ParseResult<Class>::make_ok(std::move(config));
    }
    case ParseStatus::Error:
      return ParseResult<Class>::make_err(std::move(parser).errors());
    case ParseStatus::HelpRequested:
      return ParseResult<Class>::make_help(std::move(parser).help_message());
    case ParseStatus::VersionRequested:
      return ParseResult<Class>::make_version(parser.root_command().version());
  }
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
