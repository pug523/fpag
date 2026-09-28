// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "debug/dwarf/module_cache.h"

#include "debug/dwarf/module.h"
#include "fpag/build/build_flag.h"

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)

#include <memory>
#include <string>

namespace debug::dwarf {

const Module* ModuleCache::at(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return nullptr;
  }
  for (Entry& entry : entries_) {
    if (entry.path == path) {
      return entry.module.get();
    }
  }
  entries_.push_back(Entry{std::string(path), std::make_unique<Module>(path)});
  return entries_.back().module.get();
}

}  // namespace debug::dwarf

#endif  // FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
