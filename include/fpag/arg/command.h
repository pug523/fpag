// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fpag/arg/arg.h"

namespace arg {

class Command;

/// Internal state configuration for Command
struct CommandState {
  std::string name;
  std::string version;
  std::string about;
  std::vector<Arg> args;
  std::vector<Command> subcommands;
  bool builtin_enabled = true;
};

/// Immutable Command definition (representing a command or a subcommand tree).
class Command {
 public:
  explicit Command(CommandState&& state) : state_(std::move(state)) {}
  ~Command() = default;

  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;

  Command(Command&&) noexcept = default;
  Command& operator=(Command&&) noexcept = default;

  inline std::string_view name() const& {
    return std::string_view{state_.name};
  }
  inline std::string_view version() const& {
    return std::string_view{state_.version};
  }
  inline std::string_view about() const& {
    return std::string_view{state_.about};
  }
  inline bool builtin_enabled() const { return state_.builtin_enabled; }

  inline std::span<const Arg> args() const { return state_.args; }
  inline std::span<const Command> subcommands() const {
    return state_.subcommands;
  }

  inline std::string&& name() && { return std::move(state_).name; }
  inline std::string&& version() && { return std::move(state_).version; }
  inline std::string&& about() && { return std::move(state_).about; }

  // Lookup helpers
  const Arg* find_arg_by_short(char c) const;
  const Arg* find_arg_by_long(std::string_view long_name) const;
  const Command* find_subcommand(std::string_view sub_name) const;

 private:
  CommandState state_;
};

/// Fluent builder for constructing an immutable @ref Command.
class CommandBuilder {
 public:
  explicit CommandBuilder(std::string&& name, std::string&& version = "") {
    state_.name = std::move(name);
    state_.version = std::move(version);
  }
  ~CommandBuilder() = default;

  CommandBuilder(const CommandBuilder&) = delete;
  CommandBuilder& operator=(const CommandBuilder&) = delete;

  CommandBuilder(CommandBuilder&&) noexcept = default;
  CommandBuilder& operator=(CommandBuilder&&) noexcept = default;

  inline CommandBuilder& about(std::string&& description) & {
    state_.about = std::move(description);
    return *this;
  }

  inline CommandBuilder&& about(std::string&& description) && {
    state_.about = std::move(description);
    return std::move(*this);
  }

  inline CommandBuilder& builtin_enabled(bool enabled) & {
    state_.builtin_enabled = enabled;
    return *this;
  }

  inline CommandBuilder&& builtin_enabled(bool enabled) && {
    state_.builtin_enabled = enabled;
    return std::move(*this);
  }

  inline CommandBuilder& add_arg(Arg&& arg) & {
    state_.args.push_back(std::move(arg));
    return *this;
  }

  inline CommandBuilder&& add_arg(Arg&& arg) && {
    state_.args.push_back(std::move(arg));
    return std::move(*this);
  }

  inline CommandBuilder& add_subcommand(Command&& subcommand) & {
    state_.subcommands.push_back(std::move(subcommand));
    return *this;
  }

  inline CommandBuilder&& add_subcommand(Command&& subcommand) && {
    state_.subcommands.push_back(std::move(subcommand));
    return std::move(*this);
  }

  inline Command build() && { return Command(std::move(state_)); }

 private:
  CommandState state_;
};

}  // namespace arg
