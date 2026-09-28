// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <memory>
#include <string>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

namespace debug {

namespace dwarf {
// Defined in src/debug/dwarf/module_cache.h. The reader behind it is not part
// of this header's interface, so it is named and not included.
class ModuleCache;
}  // namespace dwarf

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
  Symbolicator();
  ~Symbolicator();

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
  // The object reader, held behind a pointer because it is an implementation
  // detail of this class rather than part of what a caller sees.
  std::unique_ptr<dwarf::ModuleCache> modules_;
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  void* process_handle_ = nullptr;
  bool dbghelp_initialized_ = false;
#endif
};

}  // namespace debug
