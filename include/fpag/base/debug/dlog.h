// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_DEBUG)
#include <cstdlib>
#include <iterator>
#include <string_view>
#include <utility>

#include "fpag/base/numeric.h"
#include "fpag/str/format_util.h"
#endif

namespace base::internal {

#if FPAG_BUILD_FLAG(IS_DEBUG)
void dlog_impl(std::string_view formatted_msg,
               const char* file,
               i32 line,
               const char* func);

template <typename... Args>
inline void dlog_internal(const char* file,
                          i32 line,
                          const char* func,
                          str::format_string<Args...> fmt,
                          Args&&... args) {
  char buf[512];
  const str::format_to_n_result result =
      str::format_to_n(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
  const std::string_view formatted_msg(buf, static_cast<usize>(result.size));
  dlog_impl(formatted_msg, file, line, func);
}
#endif

}  // namespace base::internal

#if FPAG_BUILD_FLAG(IS_DEBUG)

#define DLOG(fmt, ...)              \
  ::base::internal::dlog_internal(  \
      __FILE__, __LINE__, __func__, \
      fmt __VA_OPT__(, ) __VA_ARGS__)  // NOLINT(whitespace/parens)

#else

#define DLOG(fmt, ...) \
  noop(fmt __VA_OPT__(, ) __VA_ARGS__)  // NOLINT(whitespace/parens)

#endif  // FPAG_BUILD_FLAG(IS_DEBUG)
