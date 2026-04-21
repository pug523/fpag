// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <utility>

#include "fmt/base.h"
#include "fmt/format.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(USE_FMTLIB)
#include <fmt/core.h>
#else
#include <format>
#include <iterator>
#endif

namespace str {

#if FPAG_BUILD_FLAG(USE_FMTLIB)
template <typename... Args>
using format_string = fmt::format_string<Args...>;
using format_context = fmt::format_context;

template <typename OutputIt>
using format_to_n_result = fmt::format_to_n_result<OutputIt>;

using memory_buffer = fmt::memory_buffer;

template <typename T, typename Char = char>
// NOLINTNEXTLINE(readability-identifier-naming)
struct formatter : fmt::formatter<T, Char> {};
#else
template <typename... Args>
using format_string = std::format_string<Args...>;
using format_context = std::format_context;

template <typename OutputIt>
using format_to_n_result = std::format_to_n_result<OutputIt>;

using memory_buffer = std::string;

template <typename T, typename Char = char>
// NOLINTNEXTLINE(readability-identifier-naming)
struct formatter : std::formatter<T, Char> {};
#endif

template <typename... Args>
inline constexpr std::string format(format_string<Args...> fmt,
                                    Args&&... args) {
#if FPAG_BUILD_FLAG(USE_FMTLIB)
  return fmt::format(fmt, std::forward<Args>(args)...);
#else
  return std::format(fmt, std::forward<Args>(args)...);
#endif
}

template <typename OutputIt, typename... Args>
inline constexpr format_to_n_result<OutputIt>
format_to_n(OutputIt out, usize n, format_string<Args...> fmt, Args&&... args) {
#if FPAG_BUILD_FLAG(USE_FMTLIB)
  return fmt::format_to_n(out, n, fmt, std::forward<Args>(args)...);
#else
  return std::format_to_n(out, static_cast<std::iter_difference_t<char>>(n),
                          fmt, std::forward<Args>(args)...);
#endif
}

}  // namespace str
