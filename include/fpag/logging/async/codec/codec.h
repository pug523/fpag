// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "fpag/base/numeric.h"

namespace logging {

// What a specialization has to provide: DecodedType, encode(), decode(),
// is_fixed_size(), and body_size() or encoded_size() for the body's size.
template <typename T, typename Enable = void>
struct Codec;

template <typename T>
using DecodeFunction = T (*)(const char* data, usize size);

}  // namespace logging
