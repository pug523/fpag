// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/mem/string_interner.h"

#include <string_view>

#include "base/concurrent_hash_map.h"
#include "base/mem/string_id.h"
#include "base/mem/string_pool.h"
#include "base/mem/string_pool_id.h"

namespace base {

StringId StringInterner::intern(const std::string_view str) {
  if (const StringId* existing = map_.find(str)) {
    return *existing;
  }

  // Checkpoint before appending to pool, in case another thread inserts the
  // same string.
  const StringPool::Checkpoint cp = pool_.checkpoint();

  std::string_view stored;
  const StringPoolId pool_id = pool_.append(str, &stored);

  bool inserted = false;
  const StringId* ptr = map_.try_insert(stored, pool_id, &inserted);

  if (!inserted) {
    // Another thread already inserted this string, rollback and return
    // existing.
    pool_.rollback(cp);
    const StringId* existing = map_.find(str);
    // dcheck(existing);
    if (existing) [[likely]] {
      return *existing;
    } else {
      return kInvalidStringId;
    }
  }

  if (ptr) [[likely]] {
    return *ptr;
  } else {
    return kInvalidStringId;
  }
}

}  // namespace base
