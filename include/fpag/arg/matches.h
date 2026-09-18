// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "fpag/arg/converter.h"
#include "fpag/base/result.h"

namespace arg {

class Matches {
 public:
  Matches() = default;
  ~Matches() = default;

  Matches(const Matches&) = delete;
  Matches& operator=(const Matches&) = delete;

  Matches(Matches&&) noexcept = default;
  Matches& operator=(Matches&&) noexcept = default;

  inline void add(std::string_view name, std::string_view value) {
    values_.emplace_back(name, value);
  }

  inline void add_positional(std::string_view value) {
    positionals_.push_back(value);
  }

  inline bool has(std::string_view name) const {
    for (const auto& [n, v] : values_) {
      if (n == name) {
        return true;
      }
    }
    return false;
  }

  // Retrieves the argument value safely cast to type T
  template <typename T>
  base::Result<T, GetError> get(std::string_view name) const {
    static_assert(Parsable<T>,
                  "Type T is not supported by arg::Matches. "
                  "Please specialize arg::Converter<T>.");
    for (const auto& [n, value] : values_) {
      if (n == name) {
        return Converter<T>::from_string(value);
      }
    }
    return base::make_err(GetError::OutOfRange);
  }

  template <typename T>
  base::Result<std::vector<T>, GetError> get_all(std::string_view name) const {
    static_assert(Parsable<T>,
                  "Type T is not supported by arg::Matches. "
                  "Please specialize arg::Converter<T>.");
    std::vector<T> results;
    bool found = false;

    for (const auto& [n, v] : values_) {
      if (n == name) {
        base::Result<T, GetError> res = Converter<T>::from_string(v);
        if (res.is_err()) {
          return base::make_err(std::move(res).unwrap_err());
        }
        results.push_back(std::move(res).unwrap());
        found = true;
      }
    }

    if (!found) {
      return base::make_err(GetError::OutOfRange);
    }
    return base::make_ok(std::move(results));
  }

  // Gets positional arguments
  inline const std::vector<std::string_view>& positionals() const {
    return positionals_;
  }

  // Records descent into a subcommand during parsing, outermost first.
  // Views borrow the argument storage (same lifetime rule as positionals).
  inline void select_subcommand(std::string_view name) {
    command_path_.push_back(name);
  }

  // Selected subcommand path, outermost first. Empty when no subcommand
  // was selected.
  inline std::span<const std::string_view> command_path() const {
    return command_path_;
  }

  // Innermost selected subcommand, or empty when none was selected.
  inline std::string_view selected_command() const {
    return command_path_.empty() ? std::string_view{} : command_path_.back();
  }

 private:
  // Using vector for fast contiguous memory access
  // (faster than map for small N)
  std::vector<std::pair<std::string_view, std::string_view>> values_;
  std::vector<std::string_view> positionals_;
  std::vector<std::string_view> command_path_;
};

}  // namespace arg
