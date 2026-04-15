// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "base/debug/string.h"
#include "base/numeric.h"
#include "build/attributes.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_RELEASE)
#include "base/debug/common.h"
#endif

namespace base::internal {

[[noreturn]] FPAG_COLD void check_fail_impl(const char* expr,
                                            const char* file,
                                            i32 line,
                                            const char* func,
                                            std::string_view msg = "");

[[noreturn]] FPAG_COLD void check_op_fail_impl(const char* expected,
                                               std::string_view lhs,
                                               std::string_view rhs,
                                               const char* file,
                                               i32 line,
                                               const char* func,
                                               std::string_view msg = "");

[[noreturn]] FPAG_COLD inline void check_op_fail_internal(
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

[[noreturn]] FPAG_COLD void raw_check_fail_impl(const char* expr,
                                                const char* file,
                                                i32 line,
                                                const char* func,
                                                std::string_view msg = "");

}  // namespace base::internal

#define FPAG_CHECK_EVALUATE(expr, on_failed)      \
  do {                                            \
    auto&& _expr = (expr);                        \
    /* FPAG_ASSUME(_expr); */                     \
    if (!static_cast<bool>(_expr)) [[unlikely]] { \
      on_failed;                                  \
    }                                             \
  } while (false)

#define FPAG_CHECK_OP_EVALUATE(lhs, rhs, op, failed) \
  do {                                               \
    auto&& _lhs = (lhs);                             \
    auto&& _rhs = (rhs);                             \
    /* FPAG_ASSUME(_lhs op _rhs); */                 \
    if (!(_lhs op _rhs)) [[unlikely]] {              \
      failed;                                        \
    }                                                \
  } while (false)

#define FPAG_CHECK(expr)                                       \
  FPAG_CHECK_EVALUATE(expr, ::base::internal::check_fail_impl( \
                                #expr, __FILE__, __LINE__, __func__))

#define FPAG_CHECK_MSG(expr, msg)                              \
  FPAG_CHECK_EVALUATE(expr, ::base::internal::check_fail_impl( \
                                #expr, __FILE__, __LINE__, __func__, msg))

// No heap allocations on failure.
#define FPAG_RAW_CHECK(expr)                                       \
  FPAG_CHECK_EVALUATE(expr, ::base::internal::raw_check_fail_impl( \
                                #expr, __FILE__, __LINE__, __func__))

#define FPAG_RAW_CHECK_MSG(expr, msg)                              \
  FPAG_CHECK_EVALUATE(expr, ::base::internal::raw_check_fail_impl( \
                                #expr, __FILE__, __LINE__, __func__, msg))

#define FPAG_CHECK_OP(lhs, rhs, op)             \
  FPAG_CHECK_OP_EVALUATE(                       \
      lhs, rhs, op,                             \
      ::base::internal::check_op_fail_internal( \
          #lhs " " #op " " #rhs, _lhs, _rhs, __FILE__, __LINE__, __func__))

#define FPAG_CHECK_OP_MSG(lhs, rhs, op, msg)                              \
  FPAG_CHECK_OP_EVALUATE(lhs, rhs, op,                                    \
                         ::base::internal::check_op_fail_internal(        \
                             #lhs " " #op " " #rhs, _lhs, _rhs, __FILE__, \
                             __LINE__, __func__, msg))

#define FPAG_CHECK_EQ(lhs, rhs) FPAG_CHECK_OP(lhs, rhs, ==)
#define FPAG_CHECK_NE(lhs, rhs) FPAG_CHECK_OP(lhs, rhs, !=)
#define FPAG_CHECK_LT(lhs, rhs) FPAG_CHECK_OP(lhs, rhs, <)
#define FPAG_CHECK_LE(lhs, rhs) FPAG_CHECK_OP(lhs, rhs, <=)
#define FPAG_CHECK_GT(lhs, rhs) FPAG_CHECK_OP(lhs, rhs, >)
#define FPAG_CHECK_GE(lhs, rhs) FPAG_CHECK_OP(lhs, rhs, >=)

#define FPAG_CHECK_EQ_MSG(lhs, rhs, msg) FPAG_CHECK_OP_MSG(lhs, rhs, ==, msg)
#define FPAG_CHECK_NE_MSG(lhs, rhs, msg) FPAG_CHECK_OP_MSG(lhs, rhs, !=, msg)
#define FPAG_CHECK_LT_MSG(lhs, rhs, msg) FPAG_CHECK_OP_MSG(lhs, rhs, <, msg)
#define FPAG_CHECK_LE_MSG(lhs, rhs, msg) FPAG_CHECK_OP_MSG(lhs, rhs, <=, msg)
#define FPAG_CHECK_GT_MSG(lhs, rhs, msg) FPAG_CHECK_OP_MSG(lhs, rhs, >, msg)
#define FPAG_CHECK_GE_MSG(lhs, rhs, msg) FPAG_CHECK_OP_MSG(lhs, rhs, >=, msg)

#if FPAG_BUILD_FLAG(IS_DEBUG)

#define FPAG_DCHECK(expr) FPAG_CHECK(expr)
#define FPAG_DCHECK_MSG(expr, msg) FPAG_CHECK_MSG(expr, msg)
#define FPAG_RAW_DCHECK(expr) FPAG_RAW_CHECK(expr)
#define FPAG_RAW_DCHECK_MSG(expr, msg) FPAG_RAW_CHECK_MSG(expr, msg)

#define FPAG_DCHECK_EQ(lhs, rhs) FPAG_CHECK_EQ(lhs, rhs)
#define FPAG_DCHECK_NE(lhs, rhs) FPAG_CHECK_NE(lhs, rhs)
#define FPAG_DCHECK_LT(lhs, rhs) FPAG_CHECK_LT(lhs, rhs)
#define FPAG_DCHECK_LE(lhs, rhs) FPAG_CHECK_LE(lhs, rhs)
#define FPAG_DCHECK_GT(lhs, rhs) FPAG_CHECK_GT(lhs, rhs)
#define FPAG_DCHECK_GE(lhs, rhs) FPAG_CHECK_GE(lhs, rhs)

#define FPAG_DCHECK_EQ_MSG(lhs, rhs, msg) FPAG_CHECK_EQ_MSG(lhs, rhs, msg)
#define FPAG_DCHECK_NE_MSG(lhs, rhs, msg) FPAG_CHECK_NE_MSG(lhs, rhs, msg)
#define FPAG_DCHECK_LT_MSG(lhs, rhs, msg) FPAG_CHECK_LT_MSG(lhs, rhs, msg)
#define FPAG_DCHECK_LE_MSG(lhs, rhs, msg) FPAG_CHECK_LE_MSG(lhs, rhs, msg)
#define FPAG_DCHECK_GT_MSG(lhs, rhs, msg) FPAG_CHECK_GT_MSG(lhs, rhs, msg)
#define FPAG_DCHECK_GE_MSG(lhs, rhs, msg) FPAG_CHECK_GE_MSG(lhs, rhs, msg)

#else

#define FPAG_DCHECK(expr) NOOP(expr)
#define FPAG_DCHECK_MSG(expr, msg) NOOP(expr, msg)
#define FPAG_RAW_DCHECK(expr) NOOP(expr)
#define FPAG_RAW_DCHECK_MSG(expr, msg) NOOP(expr, msg)

#define FPAG_DCHECK_EQ(lhs, rhs) NOOP(lhs, rhs)
#define FPAG_DCHECK_NE(lhs, rhs) NOOP(lhs, rhs)
#define FPAG_DCHECK_LT(lhs, rhs) NOOP(lhs, rhs)
#define FPAG_DCHECK_LE(lhs, rhs) NOOP(lhs, rhs)
#define FPAG_DCHECK_GT(lhs, rhs) NOOP(lhs, rhs)
#define FPAG_DCHECK_GE(lhs, rhs) NOOP(lhs, rhs)

#define FPAG_DCHECK_EQ_MSG(lhs, rhs, msg) NOOP(lhs, rhs, msg)
#define FPAG_DCHECK_NE_MSG(lhs, rhs, msg) NOOP(lhs, rhs, msg)
#define FPAG_DCHECK_LT_MSG(lhs, rhs, msg) NOOP(lhs, rhs, msg)
#define FPAG_DCHECK_LE_MSG(lhs, rhs, msg) NOOP(lhs, rhs, msg)
#define FPAG_DCHECK_GT_MSG(lhs, rhs, msg) NOOP(lhs, rhs, msg)
#define FPAG_DCHECK_GE_MSG(lhs, rhs, msg) NOOP(lhs, rhs, msg)

#endif  // FPAG_BUILD_FLAG(IS_DEBUG)
