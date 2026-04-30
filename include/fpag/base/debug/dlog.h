// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <iterator>

#include "fpag/base/debug/common.h"  // IWYU pragma: keep
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_DEBUG)
#include <cstdlib>
#include <string_view>

#include "fmt/compile.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "fpag/base/numeric.h"
#endif

namespace base::internal {

#if FPAG_BUILD_FLAG(IS_DEBUG)
void dlog_impl(std::string_view formatted_msg,
               const char* file,
               i32 line,
               const char* func);

template <typename CompiledFormat, typename... Args>
inline constexpr void dlog_internal(const char* file,
                                    i32 line,
                                    const char* func,
                                    CompiledFormat,
                                    Args&&... args) {
  fmt::memory_buffer out;

  // Build passes but clang-tidy fails fmt::format_to compile checking for
  // unknown reasons.
#ifndef __clang_analyzer__
  fmt::format_to(
      std::back_inserter(out), CompiledFormat{},
      std::forward<Args>(args)...);  // NOLINT(build/include_what_you_use)
#else
  // NOLINTNEXTLINE(build/include_what_you_use)
  fmt::format_to(std::back_inserter(out), "", std::forward<Args>(args)...);
#endif

  const std::string_view formatted_msg(out.data(), out.size());
  dlog_impl(formatted_msg, file, line, func);
}
#endif

}  // namespace base::internal

#if FPAG_BUILD_FLAG(IS_DEBUG)

// NOLINTBEGIN(whitespace/parens)
#define FPAG_DLOG(fmt, ...)                                     \
  ::base::internal::dlog_internal(__FILE__, __LINE__, __func__, \
                                  FMT_COMPILE(fmt) __VA_OPT__(, ) __VA_ARGS__)
// NOLINTEND(whitespace/parens)

#else

#define FPAG_DLOG(fmt, ...) \
  NOOP(fmt __VA_OPT__(, ) __VA_ARGS__)  // NOLINT(whitespace/parens)

#endif  // FPAG_BUILD_FLAG(IS_DEBUG)
