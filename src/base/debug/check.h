// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "base/debug/common.h"
#include "base/numeric.h"

namespace base::internal {

[[noreturn, gnu::cold]] void check_fail_impl(const char* expr,
                                             const char* file,
                                             i32 line,
                                             const char* func,
                                             std::string_view msg = "");

[[noreturn, gnu::cold]] void raw_check_fail_impl(const char* expr,
                                                 const char* file,
                                                 i32 line,
                                                 const char* func,
                                                 std::string_view msg = "");

}  // namespace base::internal

// internal macros
#define check_internal(expr, on_failed)           \
  do {                                            \
    auto&& _expr = (expr);                        \
    if (!static_cast<bool>(_expr)) [[unlikely]] { \
      on_failed;                                  \
    }                                             \
  } while (false)

#define check_op_internal(op, a, b)                                            \
  do {                                                                         \
    auto&& _a = (a);                                                           \
    auto&& _b = (b);                                                           \
    if (!(_a op _b)) [[unlikely]] {                                            \
      ::base::internal::check_fail_impl(#a " " #op " " #b, __FILE__, __LINE__, \
                                        __func__,                              \
                                        std::format("{} vs {}", _a, _b));      \
    }                                                                          \
  } while (false)

#define check(expr)                                                       \
  check_internal(expr, ::base::internal::check_fail_impl(#expr, __FILE__, \
                                                         __LINE__, __func__))

#define check_msg(expr, msg)                              \
  check_internal(expr, ::base::internal::check_fail_impl( \
                           #expr, __FILE__, __LINE__, __func__, msg))

// No heap allocations on failure.
#define raw_check(expr)                                       \
  check_internal(expr, ::base::internal::raw_check_fail_impl( \
                           #expr, __FILE__, __LINE__, __func__))

#define raw_check_msg(expr, msg)                              \
  check_internal(expr, ::base::internal::raw_check_fail_impl( \
                           #expr, __FILE__, __LINE__, __func__, msg))

#define check_op(op, a, b)                                                     \
  check_internal(                                                              \
      (a op b), ::base::internal::check_fail_impl(#a " " #op " " #b, __FILE__, \
                                                  __LINE__, __func__))

#define check_eq(a, b) check_op(==, a, b)
#define check_ne(a, b) check_op(!=, a, b)
#define check_lt(a, b) check_op(<, a, b)
#define check_le(a, b) check_op(<=, a, b)
#define check_gt(a, b) check_op(>, a, b)
#define check_ge(a, b) check_op(>=, a, b)

#if FPAG_IS_DEBUG

#define dcheck(expr) check(expr)
#define dcheck_msg(expr, msg) check_msg(expr, msg)
#define raw_dcheck(expr) raw_check(expr)
#define raw_dcheck_msg(expr, msg) raw_check_msg(expr, msg)

#define dcheck_eq(a, b) check_eq(==, a, b)
#define dcheck_ne(a, b) check_ne(!=, a, b)
#define dcheck_lt(a, b) check_lt(<, a, b)
#define dcheck_le(a, b) check_le(<=, a, b)
#define dcheck_gt(a, b) check_gt(>, a, b)
#define dcheck_ge(a, b) check_ge(>=, a, b)
#else
#define dcheck(expr) noop(expr)
#define dcheck_msg(expr, msg) noop(expr, msg)
#define raw_dcheck(expr) noop(expr, msg)
#define raw_dcheck_msg(expr, msg) noop(expr, msg)

#define dcheck_eq(a, b) noop(a, b)
#define dcheck_ne(a, b) noop(a, b)
#define dcheck_lt(a, b) noop(a, b)
#define dcheck_le(a, b) noop(a, b)
#define dcheck_gt(a, b) noop(a, b)
#define dcheck_ge(a, b) noop(a, b)
#endif  // FPAG_IS_DEBUG
