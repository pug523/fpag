// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
// #include "fpag/base/debug/dwarf/provider.h"
#endif

namespace base {

// Result of symbolicating a single address. All strings are owned by this
// struct (not views) because the symbolication layer may allocate them.
struct SymbolInfo {
  std::string function;  // demangled function name, or "" if unknown
  std::string file;      // source file path, or ""
  u32 line = 0;
  u32 column = 0;
  bool resolved = false;
};

class Symbolicator {
 public:
#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  Symbolicator() = default;
  ~Symbolicator() = default;
#else
  Symbolicator();
  ~Symbolicator();
#endif

  Symbolicator(const Symbolicator&) = delete;
  Symbolicator& operator=(const Symbolicator&) = delete;

  // Resolves `address` to symbol information. Demangling is applied
  // automatically via the `demangle` layer.
  SymbolInfo resolve(const void* address) const;

 private:
#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  SymbolInfo resolve_posix(const void* address) const;
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  SymbolInfo resolve_win(const void* address) const;
#endif

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
  // DwarfProvider dwarf_provider_;
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  void* process_handle_ = nullptr;
  bool dbghelp_initialized_ = false;
#endif
};

}  // namespace base
