// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>

namespace base {

// Demangles `mangled_name` into a human-readable C++ symbol name.
// Returns the demangled string on success, or a copy of `mangled_name` if
// demangling fails or is unsupported on the current platform.
// Requires `mangled_name` to be null-terminated.
//
// POSIX: abi::__cxa_demangle
// Windows: UnDecorateSymbolName
std::string demangle(const std::string& mangled_name);

}  // namespace base
