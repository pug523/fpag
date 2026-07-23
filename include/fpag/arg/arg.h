// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <optional>
#include <string_view>
#include <utility>

#include "fpag/base/numeric.h"

namespace arg {

class ArgBuilder;

// Represents a single command-line argument definition.
// Use `ArgBuilder` to construct this class.
class Arg {
 public:
  Arg() = delete;
  ~Arg() = default;

  Arg(const Arg&) = delete;
  Arg& operator=(const Arg&) = delete;

  Arg(Arg&&) noexcept = default;
  Arg& operator=(Arg&&) noexcept = default;

  constexpr std::string_view name() const { return name_; }
  constexpr std::optional<char> short_name() const { return short_name_; }
  constexpr std::string_view long_name() const { return long_name_; }
  constexpr std::string_view help() const { return help_; }
  constexpr bool is_required() const { return is_required_; }
  constexpr bool is_flag() const { return is_flag_; }

 private:
  friend class ArgBuilder;

  explicit constexpr Arg(std::string_view name,
                         std::string_view help,
                         std::string_view long_name,
                         std::optional<char> short_name,
                         bool is_required,
                         bool is_flag)
      : name_(name),
        help_(help),
        long_name_(long_name),
        short_name_(short_name),
        is_required_(is_required),
        is_flag_(is_flag) {}

  std::string_view name_;
  std::string_view help_;
  std::string_view long_name_;
  std::optional<char> short_name_;
  // Align to 64 B
  [[maybe_unused]] i32 padding_ = 0;
  bool is_required_ = false;
  bool is_flag_ = false;
};

// Fluent builder for constructing an `Arg`.
class ArgBuilder {
 public:
  explicit constexpr ArgBuilder(std::string_view name) : name_(name) {}
  ~ArgBuilder() = default;

  ArgBuilder(const ArgBuilder&) = delete;
  ArgBuilder& operator=(const ArgBuilder&) = delete;

  ArgBuilder(ArgBuilder&&) noexcept = default;
  ArgBuilder& operator=(ArgBuilder&&) noexcept = default;

  constexpr ArgBuilder& short_name(char c) & {
    short_name_ = c;
    return *this;
  }

  constexpr ArgBuilder&& short_name(char c) && {
    short_name_ = c;
    return std::move(*this);
  }

  constexpr ArgBuilder& long_name(std::string_view n) & {
    long_name_ = n;
    return *this;
  }

  constexpr ArgBuilder&& long_name(std::string_view n) && {
    long_name_ = n;
    return std::move(*this);
  }

  constexpr ArgBuilder& help(std::string_view h) & {
    help_ = h;
    return *this;
  }

  constexpr ArgBuilder&& help(std::string_view h) && {
    help_ = h;
    return std::move(*this);
  }

  constexpr ArgBuilder& required(bool r = true) & {
    is_required_ = r;
    return *this;
  }

  constexpr ArgBuilder&& required(bool r = true) && {
    is_required_ = r;
    return std::move(*this);
  }

  constexpr ArgBuilder& is_flag(bool f = true) & {
    is_flag_ = f;
    return *this;
  }

  constexpr ArgBuilder&& is_flag(bool f = true) && {
    is_flag_ = f;
    return std::move(*this);
  }

  // Finalizes construction and returns an immutable Arg object.
  constexpr Arg build() const {
    return Arg(name_, help_, long_name_, short_name_, is_required_, is_flag_);
  }

 private:
  std::string_view name_;
  std::string_view help_;
  std::string_view long_name_;
  std::optional<char> short_name_;
  bool is_required_ = false;
  bool is_flag_ = false;
};

}  // namespace arg
