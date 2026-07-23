// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <optional>
#include <string_view>

#include "fpag/base/numeric.h"

namespace arg {

// Represents a single command-line argument definition.
class Arg {
 public:
  explicit constexpr Arg(std::string_view name) : name_(name) {}
  ~Arg() = default;

  Arg(const Arg&) = delete;
  Arg& operator=(const Arg&) = delete;

  Arg(Arg&&) noexcept = default;
  Arg& operator=(Arg&&) noexcept = default;

  // Fluent API builders
  constexpr Arg& short_name(char c) {
    short_name_ = c;
    return *this;
  }

  constexpr Arg& long_name(std::string_view n) {
    long_name_ = n;
    return *this;
  }

  constexpr Arg& help(std::string_view h) {
    help_ = h;
    return *this;
  }

  constexpr Arg& required(bool r = true) {
    is_required_ = r;
    return *this;
  }

  constexpr Arg& is_flag(bool f = true) {
    is_flag_ = f;
    return *this;
  }

  constexpr std::string_view name() const { return name_; }
  constexpr std::optional<char> short_name() const { return short_name_; }
  constexpr std::string_view long_name() const { return long_name_; }
  constexpr std::string_view help() const { return help_; }
  constexpr bool is_required() const { return is_required_; }
  constexpr bool is_flag() const { return is_flag_; }

 private:
  std::string_view name_;
  std::string_view help_;
  std::string_view long_name_;
  std::optional<char> short_name_;
  [[maybe_unused]] i32 padding;
  bool is_required_ = false;
  bool is_flag_ = false;
};

}  // namespace arg
