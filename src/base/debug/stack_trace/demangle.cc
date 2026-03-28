// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/stack_trace/demangle.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "base/numeric.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <cxxabi.h>
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
#include <dbghelp.h>
#include <windows.h>
#else
#error "Unsupported platform for symbol demangling"
#endif

namespace base {

namespace {

#if FPAG_BUILD_FLAG(IS_OS_POSIX)

std::string demangle_posix(std::string_view mangled_name) {
  if (mangled_name.empty()) [[unlikely]] {
    return "";
  }

  i32 status = -1;
  char* demangled =
      abi::__cxa_demangle(mangled_name.data(), nullptr, nullptr, &status);
  if (status == 0 && demangled) {
    std::string result(demangled);
    ::free(demangled);
    return result;
  }

  if (demangled) {
    ::free(demangled);
  }

  return std::string(mangled_name);
}

#elif FPAG_BUILD_FLAG(IS_OS_WIN)

std::string demangle_win(std::string_view mangled_name) {
  if (mangled_name.empty()) [[unlikely]] {
    return {};
  }

  char output[1024] = {};
  DWORD result =
      ::UnDecorateSymbolName(mangled_name.data(), output, sizeof(output),
                             UNDNAME_COMPLETE | UNDNAME_NO_THROW_SIGNATURES);

  if (result > 0 && output[0] != '\0' && output != mangled_name.data()) {
    return std::string(output, result);
  }
  return std::string(mangled_name);
}

#endif

}  // namespace

std::string demangle(const std::string& mangled_name) {
  if (mangled_name.empty()) [[unlikely]] {
    return "";
  }

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  return demangle_posix(std::string_view{mangled_name});
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return demangle_win(std::string_view{mangled_name});
#endif
}

}  // namespace base
