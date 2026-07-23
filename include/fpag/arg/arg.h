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

struct ArgState {
  std::optional<std::string_view> name;
  std::string_view help;
  std::string_view long_name;
  std::string_view default_value;
  std::vector<std::string> choices;
  std::optional<char> short_name;
  bool is_required = false;
  bool is_flag = false;
};

// Represents a single command-line argument definition.
// Use `ArgBuilder` to construct this class.
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
  inline std::span<const std::string> choices() const { return state_.choices; }
  inline bool is_required() const { return state_.is_required; }
  inline bool is_flag() const { return state_.is_flag; }

 private:
  ArgState state_;
};

// Fluent builder for constructing an `Arg`.
class ArgBuilder {
 public:
  explicit ArgBuilder(std::string_view long_name) {
    state_.long_name = long_name;
  }
  ~ArgBuilder() = default;

  ArgBuilder(const ArgBuilder&) = delete;
  ArgBuilder& operator=(const ArgBuilder&) = delete;

  ArgBuilder(ArgBuilder&&) noexcept = default;
  ArgBuilder& operator=(ArgBuilder&&) noexcept = default;

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

  inline constexpr ArgBuilder& help(std::string_view h) & {
    state_.help = h;
    return *this;
  }

  inline constexpr ArgBuilder&& help(std::string_view h) && {
    state_.help = h;
    return std::move(*this);
  }

  inline constexpr ArgBuilder& default_value(std::string_view d) & {
    state_.default_value = d;
    return *this;
  }

  inline constexpr ArgBuilder&& default_value(std::string_view d) && {
    state_.default_value = d;
    return std::move(*this);
  }

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

  inline constexpr ArgBuilder& required(bool r = true) & {
    state_.is_required = r;
    return *this;
  }

  inline constexpr ArgBuilder&& required(bool r = true) && {
    state_.is_required = r;
    return std::move(*this);
  }

  inline constexpr ArgBuilder& is_flag(bool f = true) & {
    state_.is_flag = f;
    return *this;
  }

  inline constexpr ArgBuilder&& is_flag(bool f = true) && {
    state_.is_flag = f;
    return std::move(*this);
  }

  // Finalizes construction and returns an immutable Arg object.
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
