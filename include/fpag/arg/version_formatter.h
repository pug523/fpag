// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "fpag/base/color_style.h"
#include "fpag/base/numeric.h"

namespace arg {

class Command;

class VersionFormatter {
 public:
  VersionFormatter() = default;
  ~VersionFormatter() = default;

  VersionFormatter(const VersionFormatter&) = delete;
  VersionFormatter& operator=(const VersionFormatter&) = delete;

  VersionFormatter(VersionFormatter&&) noexcept = default;
  VersionFormatter& operator=(VersionFormatter&&) noexcept = default;

  std::string_view format(std::string_view command_name,
                          std::string_view version) &;
  std::string_view reformat(std::string_view command_name,
                            std::string_view version) &;

  inline std::string&& format(std::string_view command_name,
                              std::string_view version) && {
    format(command_name, version);
    return std::move(formatted_str_);
  }
  inline std::string&& reformat(std::string_view command_name,
                                std::string_view version) && {
    reformat(command_name, version);
    return std::move(formatted_str_);
  }

 private:
  std::string formatted_str_;
};

}  // namespace arg

