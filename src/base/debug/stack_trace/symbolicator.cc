// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/stack_trace/symbolicator.h"

#include <cstdint>
#include <cstring>
#include <utility>

#include "fmt/base.h"
#include "fpag/base/debug/stack_trace/demangle.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

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
// TODO: Add support for Linux/Android stack trace file / line / column
// resolution provided by DWARF parser.
// #include "fpag/base/debug/dwarf/provider.h"
#endif

#if FPAG_BUILD_FLAG(IS_OS_LINUX)
// TODO: Remove this section when we implement DWARF parser.
#include <stdio.h>

#include <memory>
#include <string>

#endif

namespace base {

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

#if FPAG_BUILD_FLAG(IS_OS_LINUX)
  // TODO: Remove this section when we implement DWARF parser.
  if (dl.dli_fname && dl.dli_fname[0]) {
    constexpr usize kBufSize = 512;
    char command[kBufSize];

    uintptr_t offset = reinterpret_cast<uintptr_t>(address);
    if (dl.dli_fname[0] == '/') {
      offset -= reinterpret_cast<uintptr_t>(dl.dli_fbase);
    }
    const auto result = fmt::format_to_n(
        command, sizeof(command), "addr2line -e {} -f -p -C {:x} 2>/dev/null",
        dl.dli_fname, offset);
    if (result.size < sizeof(command)) {
      command[result.size] = '\0';

      const auto deleter = [](FILE* f) { pclose(f); };
      std::unique_ptr<FILE, decltype(deleter)> pipe(popen(command, "r"),
                                                    deleter);

      if (pipe) {
        char buffer[kBufSize];

        if (fgets(buffer, sizeof(buffer), pipe.get())) {
          std::string output(buffer);
          if (!output.empty() && output.back() == '\n') {
            output.pop_back();
          }

          const usize at_pos = output.find(" at ");
          if (at_pos != std::string::npos) {
            std::string file_line = output.substr(at_pos + 4);
            const usize colon_pos = file_line.find_last_of(':');

            if (colon_pos != std::string::npos) {
              std::string file = file_line.substr(0, colon_pos);
              std::string line_str = file_line.substr(colon_pos + 1);

              if (file != "??" && !line_str.empty() && line_str[0] != '?') {
                info.file = std::move(file);
                info.line = static_cast<u32>(std::stoi(line_str));
              }
            }
          }
        }
      }
    }
  }
#endif

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
  // TODO: Add support for Linux/Android stack trace file / line / column
  // resolution provided by DWARF parser.
  //
  // const DwarfModule* module =
  //     dwarf_provider_.module(dl.dli_fname, dl.dli_fbase);
  // if (module) {
  //   const DwarfInfo dwarf = module->lookup(address);
  //   if (dwarf.found) {
  //     info.file = std::move(dwarf.file);
  //     info.line = dwarf.line;
  //     info.column = dwarf.column;
  //   }
#endif

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
#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  return resolve_posix(address);
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return resolve_win(address);
#endif
}

}  // namespace base
