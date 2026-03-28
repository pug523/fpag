// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstdint>
#include <string>

#include "build/build_config.h"

namespace base {

// Describes the loaded module (shared library or executable) that contains a
// given address.
struct ModuleInfo {
  std::string module_path;  // absolute path to the binary
  void* module_base = nullptr;
  uintptr_t offset = 0;  // address relative to module_base
};

// Looks up the module that contains `address` and fills `out`.
// Returns `true` if a module was found, `false` otherwise.
bool lookup_module_for_address(const void* address, ModuleInfo* out);

}  // namespace base
