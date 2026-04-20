// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/base/simple_concurrent_hash_map.h"
#include "fpag/base/xxh3_hasher.h"
#include "fpag/str/string_pool.h"
#include "fpag/str/string_pool_id.h"

namespace str {

// Concurrent string interner that uses a StringPool for storage.
class StringInterner {
 public:
  using Map = base::SimpleConcurrentHashMap<std::string_view,
                                            StringPoolId,
                                            base::Xxh3Hasher64>;

  using StringId = StringPoolId;

  explicit StringInterner(usize map_init_capacity) : map_(map_init_capacity) {}
  ~StringInterner() = default;

  StringInterner(const StringInterner&) = delete;
  StringInterner& operator=(const StringInterner&) = delete;

  StringInterner(StringInterner&&) noexcept = delete;
  StringInterner& operator=(StringInterner&&) noexcept = delete;

  // Interns the string and returns a stable StringId.
  StringId intern(const std::string_view str);

  inline std::string_view get(StringId id) const { return pool_.get(id); }
  inline const StringPool& pool() const { return pool_; }
  inline const Map& map() const { return map_; }

  // Returns the total size of all strings in the pool.
  inline usize size() const { return pool_.size(); }
  // Returns the number of strings in the pool.
  inline usize string_count() const { return pool_.string_count(); }

 private:
  StringPool pool_;
  Map map_;
};

}  // namespace str
