// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/stack_trace/symbolicator.h"

#include <cstring>

#include "base/debug/stack_trace/demangle.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(USE_ELFUTILS)
#include <elfutils/libdwfl.h>
#include <unistd.h>
#endif

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
// backtrace_symbols + dlfcn
#include <dlfcn.h>
#include <execinfo.h>
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
#include <dbghelp.h>
#include <windows.h>
// #pragma comment(lib, "dbghelp.lib")
#else
#error "Unsupported platform for Symbolicator"
#endif

namespace base {

#if FPAG_BUILD_FLAG(USE_ELFUTILS)

Symbolicator::Symbolicator() {
  static const Dwfl_Callbacks callbacks = {
      .find_elf = dwfl_linux_proc_find_elf,
      .find_debuginfo = dwfl_standard_find_debuginfo,
      .section_address = dwfl_offline_section_address,
      .debuginfo_path = nullptr,
  };

  dwfl_ = dwfl_begin(&callbacks);
  if (dwfl_) {
    dwfl_report_begin(dwfl_);
    dwfl_linux_proc_report(dwfl_, getpid());
    dwfl_report_end(dwfl_, nullptr, nullptr);
  }
}

Symbolicator::~Symbolicator() {
  if (dwfl_) {
    dwfl_end(dwfl_);
  }
}

SymbolInfo Symbolicator::resolve_elfutils(const void* address) const {
  SymbolInfo info;
  const uintptr_t addr = reinterpret_cast<uintptr_t>(address);

  if (dwfl_) {
    Dwfl_Module* mod = dwfl_addrmodule(dwfl_, addr);
    if (mod) {
      const char* name = dwfl_module_addrname(mod, addr);
      if (name) {
        info.function = demangle(name);
        info.resolved = true;
      }

      Dwfl_Line* line_ptr = dwfl_module_getsrc(mod, addr);
      if (line_ptr) {
        int line = 0;
        int col = 0;
        const char* file =
            dwfl_lineinfo(line_ptr, nullptr, &line, &col, nullptr, nullptr);
        if (file) {
          info.file = file;
          info.line = static_cast<u32>(line);
          info.column = static_cast<u32>(col);
        }
      }

      if (info.resolved) {
        return info;
      }
    }
    return info;
  } else {
    return resolve_posix(address);
  }
}
#endif

#if FPAG_BUILD_FLAG(IS_OS_POSIX)

SymbolInfo Symbolicator::resolve_posix(const void* address) const {
  SymbolInfo info;

  Dl_info dl = {};
  if (!::dladdr(address, &dl)) {
    return info;
  }

  if (dl.dli_sname && dl.dli_sname[0]) {
    info.function = demangle(dl.dli_sname);
    info.resolved = true;
  } else if (dl.dli_fname && dl.dli_fname[0]) {
    // At minimum we know which module it came from.
    info.resolved = true;
  }

  // dladdr does not provide file/line info. Those require DWARF parsing
  // (addr2line, llvm-symbolizer, or libdw) which is platform/toolchain
  // specific and outside the scope of this in-process layer.
  // See: symbolization via external addr2line subprocess if needed.

  return info;
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
#if FPAG_BUILD_FLAG(USE_ELFUTILS)
  return resolve_elfutils(address);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return resolve_posix(address);
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return resolve_win(address);
#endif
}

}  // namespace base
