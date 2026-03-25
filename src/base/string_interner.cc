// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/string_interner.h"

#include <string_view>
#include <utility>

#include "base/concurrent_hash_map.h"
#include "base/numeric.h"
#include "base/string_id.h"

namespace base {

StringId StringInterner::intern(const std::string_view str) {
  if (const StringId* existing = map_.find(str)) {
    return *existing;
  }

  const StringPoolId pool_id = pool_.append(str);
  const std::string_view stored = pool_.get(pool_id);

  bool inserted = false;
  const StringId* ptr = map_.try_insert(stored, pool_id, &inserted);

  if (ptr) {
    return *ptr;
  } else {
    return kInvalidStringId;
  }
}

}  // namespace base
