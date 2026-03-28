// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <vector>

#include "base/numeric.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(USE_ELFUTILS)
struct Dwfl;
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

// Symbolication context. On platforms that require initialization (e.g.
// Windows DbgHelp), constructing this object performs that initialization and
// destroying it cleans up. On POSIX platforms it is a no-op wrapper.
class Symbolicator {
 public:
#if FPAG_BUILD_FLAG(IS_OS_POSIX) && !FPAG_BUILD_FLAG(USE_ELFUTILS)
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
#if FPAG_BUILD_FLAG(USE_ELFUTILS)
  SymbolInfo resolve_elfutils(const void* address) const;
#endif

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  SymbolInfo resolve_posix(const void* address) const;
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  SymbolInfo resolve_win(const void* address) const;
#endif

#if FPAG_BUILD_FLAG(USE_ELFUTILS)
  Dwfl* dwfl_ = nullptr;
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  // DbgHelp is not thread-safe, so we store the process handle used during
  // initialization to ensure SymCleanup is called on the same handle.
  void* process_handle_ = nullptr;
  bool dbghelp_initialized_ = false;
#endif
};

}  // namespace base
