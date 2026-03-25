// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "base/concurrent_hash_map.h"
#include "base/mem/string_pool.h"
#include "base/mem/string_pool_id.h"
#include "base/numeric.h"
#include "base/xxh3_hasher.h"

namespace base {

// Concurrent string interner that uses a StringPool for storage.
class StringInterner {
 public:
  StringInterner(usize pool_init_capacity,
                 usize map_init_capacity,
                 bool use_huge_pages)
      : pool_(pool_init_capacity, use_huge_pages), map_(map_init_capacity) {}
  ~StringInterner() = default;

  StringInterner(const StringInterner&) = delete;
  StringInterner& operator=(const StringInterner&) = delete;

  StringInterner(StringInterner&&) noexcept = delete;
  StringInterner& operator=(StringInterner&&) noexcept = delete;

  // Interns the string and returns a stable StringId.
  StringPoolId intern(const std::string_view str);

  // Returns the total size of all strings in the pool.
  usize size() const { return pool_.size(); }
  // Returns the number of strings in the pool.
  usize string_count() const { return pool_.string_count(); }

 private:
  StringPool pool_;

  ConcurrentHashMap<std::string_view, StringPoolId, Xxh3Hasher64> map_;
};

}  // namespace base
