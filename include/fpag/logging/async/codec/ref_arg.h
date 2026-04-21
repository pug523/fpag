// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/str/format_util.h"

namespace logging {

template <typename T>
struct RefArg {
  explicit RefArg(const T& o) : obj(&o) {}
  ~RefArg() = default;

  const T* obj = nullptr;
};

}  // namespace logging

template <typename T>
struct str::formatter<logging::RefArg<T>> : str::formatter<T> {
  template <typename FormatContext>
  auto format(const logging::RefArg<T>& r, FormatContext& ctx) const {
    return str::formatter<T>::format(*r.obj, ctx);
  }
};

