// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "str/string_interner.h"

#include <string_view>

#include "str/string_pool.h"
#include "str/string_pool_id.h"

namespace str {

StringInterner::StringId StringInterner::intern(const std::string_view str) {
  if (const StringPoolId* existing = map_.find(str)) {
    return *existing;
  }

  std::string_view stored;
  const StringPoolId pool_id = pool_.append(str, &stored);

  bool inserted = false;
  const StringPoolId* ptr = map_.try_insert(stored, pool_id, &inserted);

  // TODO: Fix race condition (in case if another thread already inserted this
  // string).

  return *ptr;
}

}  // namespace str
