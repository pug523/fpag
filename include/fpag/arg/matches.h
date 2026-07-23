// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "fpag/base/numeric.h"
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

  enum class GetError : u8 {
    InvalidArgument,
    OutOfRange,
  };

  // Retrieves the argument value safely cast to type T
  template <typename T>
  base::Result<T, GetError> get(std::string_view name) const {
    for (const auto& [n, v] : values_) {
      if (n == name) {
        if constexpr (std::is_same_v<T, std::string_view>) {
          return base::make_ok(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
          return base::make_ok(std::string(v));
          // NOLINTNEXTLINE(readability/braces)
        } else if constexpr (std::is_integral_v<T> ||
                             std::is_floating_point_v<T>) {
          T val{};
          auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), val);
          if (ec != std::errc()) {
            return base::make_err(GetError::InvalidArgument);
          }
          return base::make_ok(std::move(val));
        } else {
          static_assert(sizeof(T) == 0, "Unsupported type for Matches::get<T>");
        }
      }
    }
    return base::make_err(GetError::OutOfRange);
  }

  // Gets positional arguments
  inline const std::vector<std::string_view>& positionals() const {
    return positionals_;
  }

 private:
  // Using vector for fast contiguous memory access
  // (faster than map for small N)
  std::vector<std::pair<std::string_view, std::string_view>> values_;
  std::vector<std::string_view> positionals_;
};

}  // namespace arg
