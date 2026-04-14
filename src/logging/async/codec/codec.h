// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "base/numeric.h"

namespace logging {

template <typename T, typename Enable = void>
struct Codec;

template <typename T>
using DecodeFunction = T (*)(const char* data, usize size);

}  // namespace logging
