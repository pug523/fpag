// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "build/build_flag.h"

#if FPAG_BUILDFLAG(IS_DEBUG)
#include <cstdlib>
#include <format>
#include <iostream>
#include <utility>

#include "base/log_prefix.h"
#include "base/numeric.h"
#endif

#if FPAG_BUILDFLAG(IS_DEBUG)
namespace base::internal {

void dlog_impl(const char* file,
               i32 line,
               const char* func,
               std::string_view fmt,
               std::format_args args);
template <typename... Args>
inline void dlog_internal(const char* file,
                          i32 line,
                          const char* func,
                          std::format_string<Args...> fmt,
                          Args&&... args) {
  internal::dlog_impl(file, line, func, fmt.get(),
                      std::make_format_args(args...));
}

}  // namespace base::internal
#endif

#if FPAG_BUILDFLAG(IS_DEBUG)
#define dlog(fmt, ...)              \
  ::base::internal::dlog_internal(  \
      __FILE__, __LINE__, __func__, \
      fmt __VA_OPT__(, ) __VA_ARGS__)  // NOLINT(whitespace/parens)
#define dvar(...) dlog(dump_vars(__VA_ARGS__))
#else
#define dlog(fmt, ...) \
  noop(fmt __VA_OPT__(, ) __VA_ARGS__)  // NOLINT(whitespace/parens)
#define dvar(...) noop(__VA_ARGS__)
#endif  // FPAG_BUILDFLAG(IS_DEBUG)
