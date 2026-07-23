// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arg {

/// Holds the internal configuration state for a command-line argument.
struct ArgState {
  std::optional<std::string_view> name;
  std::string_view help;
  std::string_view long_name;
  std::string_view default_value;
  std::string_view value_name;
  std::vector<std::string> choices;
  std::optional<char> short_name;
  bool is_required = false;
  bool is_flag = false;
};

/// Represents an immutable command-line argument definition.
/// Use @ref ArgBuilder to construct instances of this class.
class Arg {
 public:
  explicit Arg(ArgState&& state) : state_(std::move(state)) {}
  ~Arg() = default;

  Arg(const Arg&) = delete;
  Arg& operator=(const Arg&) = delete;

  Arg(Arg&&) noexcept = default;
  Arg& operator=(Arg&&) noexcept = default;

  inline std::string_view name() const {
    return state_.name.value_or(state_.long_name);
  }
  inline std::optional<char> short_name() const { return state_.short_name; }
  inline std::string_view long_name() const { return state_.long_name; }
  inline std::string_view help() const { return state_.help; }
  inline std::string_view default_value() const { return state_.default_value; }
  inline std::string_view value_name() const { return state_.value_name; }
  inline std::span<const std::string> choices() const { return state_.choices; }
  inline bool is_required() const { return state_.is_required; }
  inline bool is_flag() const { return state_.is_flag; }

 private:
  ArgState state_;
};

/// Fluent builder for constructing an @ref Arg instance.
class ArgBuilder {
 public:
  /// @param long_name The primary long flag name (e.g., "output").
  explicit ArgBuilder(std::string_view long_name) {
    state_.long_name = long_name;
  }
  ~ArgBuilder() = default;

  ArgBuilder(const ArgBuilder&) = delete;
  ArgBuilder& operator=(const ArgBuilder&) = delete;

  ArgBuilder(ArgBuilder&&) noexcept = default;
  ArgBuilder& operator=(ArgBuilder&&) noexcept = default;

  /// Sets the single-character short flag (e.g., 'o' for -o).
  inline constexpr ArgBuilder& short_name(char c) & {
    state_.short_name = c;
    return *this;
  }

  inline constexpr ArgBuilder&& short_name(char c) && {
    state_.short_name = c;
    return std::move(*this);
  }

  inline constexpr ArgBuilder& long_name(std::string_view n) & {
    state_.long_name = n;
    return *this;
  }

  inline constexpr ArgBuilder&& long_name(std::string_view n) && {
    state_.long_name = n;
    return std::move(*this);
  }

  inline constexpr ArgBuilder& name(std::string_view n) & {
    state_.name = n;
    return *this;
  }

  inline constexpr ArgBuilder&& name(std::string_view n) && {
    state_.name = n;
    return std::move(*this);
  }

  /// Sets the help/description string displayed in CLI help output.
  inline constexpr ArgBuilder& help(std::string_view h) & {
    state_.help = h;
    return *this;
  }

  inline constexpr ArgBuilder&& help(std::string_view h) && {
    state_.help = h;
    return std::move(*this);
  }

  /// Sets the default value if the argument is not supplied by the user.
  inline constexpr ArgBuilder& default_value(std::string_view d) & {
    state_.default_value = d;
    return *this;
  }

  inline constexpr ArgBuilder&& default_value(std::string_view d) && {
    state_.default_value = d;
    return std::move(*this);
  }

  /// Sets the placeholder name for the value in help text (e.g., "FILE" in
  /// --output FILE).
  inline constexpr ArgBuilder& value_name(std::string_view v) & {
    state_.value_name = v;
    return *this;
  }

  inline constexpr ArgBuilder&& value_name(std::string_view v) && {
    state_.value_name = v;
    return std::move(*this);
  }

  /// Restricts valid input options to a fixed list of allowed strings.
  inline ArgBuilder& choices(std::vector<std::string>&& c) & {
    state_.choices = std::move(c);
    return *this;
  }

  inline ArgBuilder&& choices(std::vector<std::string>&& c) && {
    state_.choices = std::move(c);
    return std::move(*this);
  }

  inline ArgBuilder& choices(std::initializer_list<std::string> c) & {
    state_.choices.assign(c.begin(), c.end());
    return *this;
  }

  inline ArgBuilder&& choices(std::initializer_list<std::string> c) && {
    state_.choices.assign(c.begin(), c.end());
    return std::move(*this);
  }

  inline ArgBuilder& choices(std::span<const std::string> c = {}) & {
    state_.choices.assign(c.begin(), c.end());
    return *this;
  }

  inline ArgBuilder&& choices(std::span<const std::string> c = {}) && {
    state_.choices.assign(c.begin(), c.end());
    return std::move(*this);
  }

  /// Marks whether this argument must be provided by the user.
  inline constexpr ArgBuilder& required(bool r = true) & {
    state_.is_required = r;
    return *this;
  }

  inline constexpr ArgBuilder&& required(bool r = true) && {
    state_.is_required = r;
    return std::move(*this);
  }

  /// Marks whether this argument is a boolean flag (takes no value).
  inline constexpr ArgBuilder& is_flag(bool f = true) & {
    state_.is_flag = f;
    return *this;
  }

  inline constexpr ArgBuilder&& is_flag(bool f = true) && {
    state_.is_flag = f;
    return std::move(*this);
  }

  /// Finalizes construction and returns an immutable @ref Arg object.
  inline Arg build() && {
    if (!state_.name.has_value() && !state_.long_name.empty()) {
      state_.name = state_.long_name;
    }
    return Arg(std::move(state_));
  }

 private:
  ArgState state_;
};

}  // namespace arg
