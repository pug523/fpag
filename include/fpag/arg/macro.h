// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <utility>

#include "fpag/arg/arg.h"
#include "fpag/arg/matches.h"
#include "fpag/arg/parse_result.h"
#include "fpag/arg/parse_status.h"
#include "fpag/arg/parser.h"
#include "fpag/base/numeric.h"

namespace arg::detail {

// A binder object that knows how to map an arg::Arg to a struct member.
template <typename Class, typename T>
struct ArgBinder {
  Arg arg;
  T Class::* member;

  // Registers the argument to the Parser builder.
  inline void apply_to_parser(Parser* parser) && {
    // SAFETY: This is assumed to be called only by macros, and they have
    // parser instance so its pointer can not be null.
    parser->add_arg(std::move(arg));
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
                                         Parser&& parser,
                                         Binders&&... binders) {
  (std::forward<Binders>(binders).apply_to_parser(&parser), ...);

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
      return ParseResult<Class>::make_help(parser.help_message());
    case ParseStatus::VersionRequested:
      return ParseResult<Class>::make_version(parser.version());
  }
}

}  // namespace arg::detail

// Defines an option argument mapping.
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

// Generates a parse function for a user-defined struct.
#define ARGS_FN_DEFINE(Class, FnName, CommandName, Version, About, ...)        \
  inline ::arg::ParseResult<Class> FnName(i32 argc, const char* const* argv) { \
    return ::arg::detail::parse_macro_impl<Class>(                             \
        argc, argv,                                                            \
        std::move(::arg::Parser(CommandName, Version).about(About)),           \
        __VA_ARGS__);                                                          \
  }
