// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fmt/base.h"

namespace logging {

template <typename T>
struct RefArg {
  explicit RefArg(const T& o) : obj(&o) {}
  ~RefArg() = default;

  const T* obj = nullptr;
};

}  // namespace logging

template <typename T>
struct fmt::formatter<logging::RefArg<T>> : fmt::formatter<T> {
  template <typename FormatContext>
  auto format(const logging::RefArg<T>& r, FormatContext& ctx) const {
    return fmt::formatter<T>::format(*r.obj, ctx);
  }
};

