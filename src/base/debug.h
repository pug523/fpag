// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#if LIBP_IS_DEBUG
#include <cstdlib>
#include <format>
#include <iostream>
#include <utility>

#include "base/log_prefix.h"
#include "base/numeric.h"
#endif

#if LIBP_IS_DEBUG
namespace base::internal {
void dlog_impl(const char* file,
               i32 line,
               const char* func,
               std::string_view fmt,
               std::format_args args);

[[noreturn]] void dcheck_fail_impl(const char* expr,
                                   const char* file,
                                   i32 line,
                                   const char* func,
                                   std::string_view msg = "");
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

#define noop(...)              \
  do {                         \
    (void)sizeof(__VA_ARGS__); \
  } while (false)

#if LIBP_IS_DEBUG
#define on_debug(expr) expr
#define on_debug_raw(expr) on_debug(expr)
#define dlog(fmt, ...)                                          \
  ::base::internal::dlog_internal(__FILE__, __LINE__, __func__, \
                                  fmt __VA_OPT__(, ) __VA_ARGS__)  // NOLINT
#define dvar(...) dlog(dump_vars(__VA_ARGS__))
#define dcheck(expr)                                                           \
  do {                                                                         \
    if (!static_cast<bool>(expr)) [[unlikely]] {                               \
      ::base::internal::dcheck_fail_impl(#expr, __FILE__, __LINE__, __func__); \
    }                                                                          \
  } while (false)
#define dcheck_msg(expr, msg)                                                 \
  do {                                                                        \
    if (!static_cast<bool>(expr)) [[unlikely]] {                              \
      ::base::internal::dcheck_fail_impl(#expr, __FILE__, __LINE__, __func__, \
                                         msg);                                \
    }                                                                         \
  } while (false)
#define dcheck_raw(expr) dcheck(expr)
#else
#define on_debug(expr) noop(expr)
#define on_debug_raw(expr)
#define dlog(fmt, ...) noop(fmt __VA_OPT__(, ) __VA_ARGS__)  // NOLINT
#define dvar(...) noop(__VA_ARGS__)
#define dcheck(expr) noop(expr)
#define dcheck_msg(expr, msg) noop(expr, msg)
#define dcheck_raw(expr)
#endif
