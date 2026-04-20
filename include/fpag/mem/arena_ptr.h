// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <memory>

#include "fpag/mem/arena_deleter.h"

namespace mem {

template <typename T>
using ArenaUniquePtr = std::unique_ptr<T, ArenaDeleter<T>>;

}
