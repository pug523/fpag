// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "base/debug/assume.h"
#include "base/debug/string.h"
#include "base/numeric.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_RELEASE)
#include "base/debug/common.h"
#endif

namespace base::internal {

[[noreturn, gnu::cold]] void check_fail_impl(const char* expr,
                                             const char* file,
                                             i32 line,
                                             const char* func,
                                             std::string_view msg = "");

[[noreturn, gnu::cold]] void check_op_fail_impl(const char* expected,
                                                std::string_view lhs,
                                                std::string_view rhs,
                                                const char* file,
                                                i32 line,
                                                const char* func,
                                                std::string_view msg = "");

[[noreturn, gnu::cold]] inline void check_op_fail_internal(
    const char* expected,
    const auto& lhs,
    const auto& rhs,
    const char* file,
    i32 line,
    const char* func,
    std::string_view msg = "") {
  auto lhs_str = to_debug_string(lhs);
  auto rhs_str = to_debug_string(rhs);
  check_op_fail_impl(expected, std::string_view{lhs_str},
                     std::string_view{rhs_str}, file, line, func, msg);
}

[[noreturn, gnu::cold]] void raw_check_fail_impl(const char* expr,
                                                 const char* file,
                                                 i32 line,
                                                 const char* func,
                                                 std::string_view msg = "");

}  // namespace base::internal

#define check_evaluate(expr, on_failed)           \
  do {                                            \
    auto&& _expr = (expr);                        \
    FPAG_ASSUME(_expr);                           \
    if (!static_cast<bool>(_expr)) [[unlikely]] { \
      on_failed;                                  \
    }                                             \
  } while (false)

#define check_op_evaluate(op, lhs, rhs, failed) \
  do {                                          \
    auto&& _lhs = (lhs);                        \
    auto&& _rhs = (rhs);                        \
    FPAG_ASSUME(_lhs op _rhs);                  \
    if (!(_lhs op _rhs)) [[unlikely]] {         \
      failed;                                   \
    }                                           \
  } while (false)

#define check(expr)                                                       \
  check_evaluate(expr, ::base::internal::check_fail_impl(#expr, __FILE__, \
                                                         __LINE__, __func__))

#define check_msg(expr, msg)                              \
  check_evaluate(expr, ::base::internal::check_fail_impl( \
                           #expr, __FILE__, __LINE__, __func__, msg))

// No heap allocations on failure.
#define raw_check(expr)                                       \
  check_evaluate(expr, ::base::internal::raw_check_fail_impl( \
                           #expr, __FILE__, __LINE__, __func__))

#define raw_check_msg(expr, msg)                              \
  check_evaluate(expr, ::base::internal::raw_check_fail_impl( \
                           #expr, __FILE__, __LINE__, __func__, msg))

#define check_op(op, lhs, rhs)                  \
  check_op_evaluate(                            \
      op, lhs, rhs,                             \
      ::base::internal::check_op_fail_internal( \
          #lhs " " #op " " #rhs, _lhs, _rhs, __FILE__, __LINE__, __func__))

#define check_op_msg(op, lhs, rhs, msg)                                        \
  check_op_evaluate(op, lhs, rhs,                                              \
                    ::base::internal::check_op_fail_internal(                  \
                        #lhs " " #op " " #rhs, _lhs, _rhs, __FILE__, __LINE__, \
                        __func__, msg))

#define check_eq(lhs, rhs) check_op(==, lhs, rhs)
#define check_ne(lhs, rhs) check_op(!=, lhs, rhs)
#define check_lt(lhs, rhs) check_op(<, lhs, rhs)
#define check_le(lhs, rhs) check_op(<=, lhs, rhs)
#define check_gt(lhs, rhs) check_op(>, lhs, rhs)
#define check_ge(lhs, rhs) check_op(>=, lhs, rhs)

#define check_eq_msg(lhs, rhs, msg) check_op_msg(==, lhs, rhs, msg)
#define check_ne_msg(lhs, rhs, msg) check_op_msg(!=, lhs, rhs, msg)
#define check_lt_msg(lhs, rhs, msg) check_op_msg(<, lhs, rhs, msg)
#define check_le_msg(lhs, rhs, msg) check_op_msg(<=, lhs, rhs, msg)
#define check_gt_msg(lhs, rhs, msg) check_op_msg(>, lhs, rhs, msg)
#define check_ge_msg(lhs, rhs, msg) check_op_msg(>=, lhs, rhs, msg)

#if FPAG_BUILD_FLAG(IS_DEBUG)

#define dcheck(expr) check(expr)
#define dcheck_msg(expr, msg) check_msg(expr, msg)
#define raw_dcheck(expr) raw_check(expr)
#define raw_dcheck_msg(expr, msg) raw_check_msg(expr, msg)

#define dcheck_eq(lhs, rhs) check_eq(lhs, rhs)
#define dcheck_ne(lhs, rhs) check_ne(lhs, rhs)
#define dcheck_lt(lhs, rhs) check_lt(lhs, rhs)
#define dcheck_le(lhs, rhs) check_le(lhs, rhs)
#define dcheck_gt(lhs, rhs) check_gt(lhs, rhs)
#define dcheck_ge(lhs, rhs) check_ge(lhs, rhs)

#define dcheck_eq_msg(lhs, rhs, msg) check_eq_msg(lhs, rhs, msg)
#define dcheck_ne_msg(lhs, rhs, msg) check_ne_msg(lhs, rhs, msg)
#define dcheck_lt_msg(lhs, rhs, msg) check_lt_msg(lhs, rhs, msg)
#define dcheck_le_msg(lhs, rhs, msg) check_le_msg(lhs, rhs, msg)
#define dcheck_gt_msg(lhs, rhs, msg) check_gt_msg(lhs, rhs, msg)
#define dcheck_ge_msg(lhs, rhs, msg) check_ge_msg(lhs, rhs, msg)

#else

#define dcheck(expr) noop(expr)
#define dcheck_msg(expr, msg) noop(expr, msg)
#define raw_dcheck(expr) noop(expr, msg)
#define raw_dcheck_msg(expr, msg) noop(expr, msg)

#define dcheck_eq(lhs, rhs) noop(lhs, rhs)
#define dcheck_ne(lhs, rhs) noop(lhs, rhs)
#define dcheck_lt(lhs, rhs) noop(lhs, rhs)
#define dcheck_le(lhs, rhs) noop(lhs, rhs)
#define dcheck_gt(lhs, rhs) noop(lhs, rhs)
#define dcheck_ge(lhs, rhs) noop(lhs, rhs)

#define dcheck_eq_msg(lhs, rhs, msg) noop(lhs, rhs, msg)
#define dcheck_ne_msg(lhs, rhs, msg) noop(lhs, rhs, msg)
#define dcheck_lt_msg(lhs, rhs, msg) noop(lhs, rhs, msg)
#define dcheck_le_msg(lhs, rhs, msg) noop(lhs, rhs, msg)
#define dcheck_gt_msg(lhs, rhs, msg) noop(lhs, rhs, msg)
#define dcheck_ge_msg(lhs, rhs, msg) noop(lhs, rhs, msg)

#endif  // FPAG_BUILD_FLAG(IS_DEBUG)
