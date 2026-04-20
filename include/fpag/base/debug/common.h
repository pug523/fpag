// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

// #define NOOP(...) (void)(0)
#define NOOP(...) (void)sizeof(__VA_ARGS__)
