// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/stack_trace/symbolicator.h"

#include <cstdint>
#include <memory>

#include "debug/dwarf/module.h"
#include "debug/dwarf/reader.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/debug/stack_trace/demangle.h"

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <dlfcn.h>
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
// <dbghelp.h> must be included after <windows.h>.
// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#else
#error "Unsupported platform for Symbolicator"
#endif

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
#include "debug/dwarf/module_cache.h"
#endif

namespace debug {

#if FPAG_BUILD_FLAG(IS_OS_POSIX)

Symbolicator::Symbolicator() {
#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
  modules_ = std::make_unique<dwarf::ModuleCache>();
#endif
}

Symbolicator::~Symbolicator() = default;

SymbolInfo Symbolicator::resolve_posix(const void* address) const {
  SymbolInfo info;

#if FPAG_BUILD_FLAG(IS_OS_ASMJS)
  // Emscripten provides no dladdr; symbol resolution is unsupported.
  (void)address;
  return info;
#else
  Dl_info dl = {};
  if (!::dladdr(address, &dl)) {
    return info;
  }

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
  // The object's own tables are the better answer: they name the symbols the
  // dynamic table leaves out, and they carry the line the frame came from.
  if (dl.dli_fname != nullptr && dl.dli_fname[0] != '\0') {
    const dwarf::Module* const module = modules_->at(dl.dli_fname);
    if (module != nullptr && module->loaded()) {
      const u64 link_address =
          static_cast<u64>(reinterpret_cast<uintptr_t>(address)) -
          module->load_bias_to(
              static_cast<u64>(reinterpret_cast<uintptr_t>(dl.dli_fbase)));

      if (const char* const name = module->function_at(link_address)) {
        info.function = demangle(name);
        info.resolved = true;
      }

      dwarf::SourcePosition position = {};
      if (module->source_at(link_address, &position)) {
        info.file = position.file;
        info.line = position.line;
        info.column = position.column;
      }
    }
  }
#endif

  // What the dynamic table can still add: a name for an object this reader
  // cannot read, such as one stripped of its symbol table.
  if (!info.resolved && dl.dli_sname != nullptr && dl.dli_sname[0] != '\0') {
    info.function = demangle(dl.dli_sname);
    info.resolved = true;
  }
  if (!info.resolved && dl.dli_fname != nullptr && dl.dli_fname[0] != '\0') {
    // At minimum we know which module it came from.
    info.resolved = true;
  }

  return info;
#endif
}

#elif FPAG_BUILD_FLAG(IS_OS_WIN)

Symbolicator::Symbolicator() {
  process_handle_ = ::GetCurrentProcess();
  ::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES |
                  SYMOPT_NO_PROMPTS);
  dbghelp_initialized_ = ::SymInitialize(process_handle_, nullptr, TRUE);
}

Symbolicator::~Symbolicator() {
  if (dbghelp_initialized_) {
    ::SymCleanup(process_handle_);
  }
}

SymbolInfo Symbolicator::resolve_win(const void* address) const {
  SymbolInfo info;
  if (!dbghelp_initialized_) {
    return info;
  }

  const DWORD64 addr64 = reinterpret_cast<DWORD64>(address);

  // Function name
  alignas(SYMBOL_INFO) char sym_buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
  SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(sym_buf);
  sym->SizeOfStruct = sizeof(SYMBOL_INFO);
  sym->MaxNameLen = MAX_SYM_NAME;
  DWORD64 displacement = 0;

  if (::SymFromAddr(process_handle_, addr64, &displacement, sym)) {
    info.function = demangle(sym->Name);
    info.resolved = true;
  }

  // File / line
  IMAGEHLP_LINE64 line_info = {};
  line_info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
  DWORD line_displacement = 0;

  if (::SymGetLineFromAddr64(process_handle_, addr64, &line_displacement,
                             &line_info)) {
    if (line_info.FileName) {
      info.file = line_info.FileName;
    }
    info.line = static_cast<u32>(line_info.LineNumber);
  }

  return info;
}

#endif

SymbolInfo Symbolicator::resolve(const void* address) const {
#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  return resolve_posix(address);
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return resolve_win(address);
#endif
}

}  // namespace debug
