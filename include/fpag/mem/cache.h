// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <new>

#include "fpag/base/numeric.h"

namespace mem {

constexpr usize kCacheLineSize = std::hardware_destructive_interference_size;

}  // namespace mem
