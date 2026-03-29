// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include "base/numeric.h"

namespace mem {

void* reserve_pages(usize size);
bool commit_pages(void* ptr, usize size);
void decommit_pages(void* ptr, usize size);

void* allocate_pages(usize size);
void* allocate_huge_pages(usize size);
void free_pages(void* ptr, usize size);

static constexpr u64 kPageSize = 4096;                 // 4 KiB
static constexpr u64 kHugePageSize = 2 * 1024 * 1024;  // 2 MiB

}  // namespace mem
