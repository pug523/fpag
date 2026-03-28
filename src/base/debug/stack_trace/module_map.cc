// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/stack_trace/module_map.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "base/numeric.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
#include <dlfcn.h>
#include <stdio.h>
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
#include <psapi.h>
#include <windows.h>
#elif FPAG_BUILD_FLAG(IS_OS_APPLE)
#include <dlfcn.h>
#else
#error "Unsupported platform for module lookup"
#endif

namespace base {

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)

// Prefer dladdr when available (fast path). Falls back to /proc/self/maps for
// addresses in the main executable which dladdr may not resolve.
bool lookup_module_for_address_linux(const void* address, ModuleInfo* out) {
  Dl_info info = {};
  if (::dladdr(address, &info) && info.dli_fname && info.dli_fbase) {
    out->module_path = info.dli_fname;
    out->module_base = info.dli_fbase;
    out->offset = reinterpret_cast<uintptr_t>(address) -
                  reinterpret_cast<uintptr_t>(info.dli_fbase);
    return true;
  }

  // Fallback: parse /proc/self/maps
  FILE* maps = ::fopen("/proc/self/maps", "r");
  if (!maps) {
    return false;
  }

  const uintptr_t target = reinterpret_cast<uintptr_t>(address);
  char line[512];
  bool found = false;

  while (::fgets(line, sizeof(line), maps)) {
    uintptr_t start = 0, end = 0;
    char perms[8] = {};
    uintptr_t file_offset = 0;
    u32 dev_major = 0;
    u32 dev_minor = 0;
    u64 inode = 0;
    char path[256] = {};

    // Format: start-end perms offset dev inode [path]
    i32 parsed =
        ::sscanf(line, "%lx-%lx %7s %lx %x:%x %lu %255s", &start, &end, perms,
                 &file_offset, &dev_major, &dev_minor, &inode, path);
    if (parsed < 5) {
      continue;
    }
    if (target < start || target >= end) {
      continue;
    }

    out->module_path = (parsed >= 8 && path[0]) ? path : "";
    out->module_base = reinterpret_cast<void*>(start);
    out->offset = target - start;
    found = true;
    break;
  }

  ::fclose(maps);
  return found;
}

#elif FPAG_BUILD_FLAG(IS_OS_WIN)

bool lookup_module_for_address_win(const void* address, ModuleInfo* out) {
  HMODULE module = nullptr;
  if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(address), &module)) {
    return false;
  }

  wchar_t path_w[MAX_PATH] = {};
  if (!::GetModuleFileNameW(module, path_w, MAX_PATH)) {
    return false;
  }

  // Convert wide string to narrow for storage.
  char path[MAX_PATH] = {};
  ::WideCharToMultiByte(CP_UTF8, 0, path_w, -1, path, MAX_PATH, nullptr,
                        nullptr);

  out->module_path = path;
  out->module_base = reinterpret_cast<void*>(module);
  out->offset = reinterpret_cast<uintptr_t>(address) -
                reinterpret_cast<uintptr_t>(module);
  return true;
}

#elif FPAG_BUILD_FLAG(IS_OS_APPLE)

bool lookup_module_for_address_apple(const void* address, ModuleInfo* out) {
  Dl_info info = {};
  if (!::dladdr(address, &info) || !info.dli_fname || !info.dli_fbase) {
    return false;
  }

  out->module_path = info.dli_fname;
  out->module_base = info.dli_fbase;
  out->offset = reinterpret_cast<uintptr_t>(address) -
                reinterpret_cast<uintptr_t>(info.dli_fbase);
  return true;
}

#endif

bool lookup_module_for_address(const void* address, ModuleInfo* out) {
#if FPAG_BUILD_FLAG(IS_OS_LINUX)
  return lookup_module_for_address_linux(address, out);
#elif FPAG_BUILD_FLAG(IS_OS_WIN)
  return lookup_module_for_address_win(address, out);
#elif FPAG_BUILD_FLAG(IS_OS_APPLE)
  return lookup_module_for_address_apple(address, out);
#endif
}

}  // namespace base
