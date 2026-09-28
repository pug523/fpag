// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)

#include "debug/dwarf/module.h"

namespace debug::dwarf {

// Remembers the objects a symbolication pass has already read.
//
// Opening an object is not cheap: it maps the file, reads its section table,
// sorts its symbol table and walks its debug information. A stack trace asks
// about the same handful of objects over and over, so a cache turns a lookup
// into an offset and a binary search. The cache is as long lived as the pass
// that owns it and is not shared, so nothing here needs to be thread safe.
class ModuleCache {
 public:
  ModuleCache() = default;
  ~ModuleCache() = default;

  ModuleCache(const ModuleCache&) = delete;
  ModuleCache& operator=(const ModuleCache&) = delete;

  // The view of the object at `path`, read the first time it is asked for and
  // remembered after that. The answer is never null; a file that is not an
  // object this reader reads gives a module that is not loaded.
  [[nodiscard]] const Module* at(const char* path);

 private:
  struct Entry {
    std::string path;
    std::unique_ptr<Module> module;
  };

  std::vector<Entry> entries_;
};

}  // namespace debug::dwarf

#endif  // FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)
