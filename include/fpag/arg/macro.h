// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/arg/arg.h"
#include "fpag/arg/command.h"
#include "fpag/arg/parse_result.h"
#include "matches.h"

namespace arg::detail {

// A binder object that knows how to map an arg::Arg to a struct member.
template <typename Class, typename T>
struct ArgBinder {
  Arg arg;
  T Class::* member;

  // Registers the argument to the Command builder.
  inline void apply_to_command(Command* command) && {
    command->add_arg(std::move(arg));
  }

  // Extracts the parsed value from Matches into the struct member.
  inline void extract(Class* obj, const Matches& matches) const {
    // SAFETY: this is assumed to be called only by macros, and they have
    // obj instance so obj pointer can not be null
    if (arg.is_flag()) {
      if constexpr (std::is_same_v<T, bool>) {
        obj->*member = matches.has(arg.name());
      }
    } else {
      if (matches.has(arg.name())) {
        obj->*member = matches.get<T>(arg.name());
      }
    }
  }
};

template <typename Class, typename... Binders>
arg::ParseResult<Class> parse_macro_impl(i32 argc,
                                         const char* const* argv,
                                         Command command,
                                         Binders&&... binders) {
  (binders.apply_to_command(&command), ...);

  Matches matches;
  ParseStatus status = command.parse(argc, argv, &matches);

  if (status == ParseStatus::HelpRequested) {
    return base::make_err(arg::ParseError{arg::ParseError::Kind::HelpRequested,
                                          command.help_message()});
  }

  if (status == ParseStatus::Error) {
    return base::make_err(
        arg::ParseError{arg::ParseError::Kind::Error, command.error_message()});
  }

  Class config{};
  (binders.extract(&config, matches), ...);
  return config;
}

}  // namespace arg::detail

// Defines an option argument mapping.
#define ARGS_OPT(Class, Field, Type, Short, Long, Help, Required)             \
  ::arg::detail::ArgBinder<Class, Type> {                                     \
    ::arg::Arg(#Field).short_name(Short).long_name(Long).help(Help).required( \
        Required),                                                            \
        &Class::Field                                                         \
  }

// Defines a boolean flag mapping.
#define ARGS_FLAG(Class, Field, Short, Long, Help)                           \
  ::arg::detail::ArgBinder<Class, bool> {                                    \
    ::arg::Arg(#Field).short_name(Short).long_name(Long).help(Help).is_flag( \
        true),                                                               \
        &Class::Field                                                        \
  }

// Generates a static parse() method for a user-defined struct.
#define ARGS_DEFINE(Class, CommandName, Version, About, ...)           \
  inline Class parse_##Class(i32 argc, const char* const* argv) {      \
    return ::arg::detail::parse_macro_impl<Class>(                     \
        argc, argv, ::arg::Command(CommandName, Version).about(About), \
        __VA_ARGS__);                                                  \
  }
