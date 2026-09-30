// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/str/intern_table.h"
#include "fpag/str/string_pool.h"
#include "fpag/str/string_pool_id.h"

namespace str {

// Concurrent string interner that uses a StringPool for storage.
class StringInterner {
 public:
  using StringId = StringPoolId;

  // @p names is how many distinct names the table is sized for. Zero takes
  // InternTable::DEFAULT_NAMES, which is a reservation rather than an
  // allocation: a caller that does not know how many names it will intern does
  // not have to say. @p pool_bytes is how much storage the pool reserves, and
  // has to be page aligned. Zero takes StringPool::DEFAULT_POOL_CAPACITY, which
  // is 1 GiB on 64-bit but 64 MiB on 32-bit: a caller on a small address space,
  // or one that does not need that much of it, names a smaller pool.
  explicit StringInterner(usize names = 0, usize pool_bytes = 0)
      : pool_(pool_bytes == 0 ? StringPool::DEFAULT_POOL_CAPACITY : pool_bytes),
        table_(&pool_, names) {}
  ~StringInterner() = default;

  StringInterner(const StringInterner&) = delete;
  StringInterner& operator=(const StringInterner&) = delete;

  StringInterner(StringInterner&&) noexcept = delete;
  StringInterner& operator=(StringInterner&&) noexcept = delete;

  // Sizes the table for @p names, which releases the region it had. Runs before
  // the first intern, because a table cannot be resized while it is being read.
  void init(usize names) { table_.reserve(names); }

  // Interns the string and returns a stable StringId.
  StringId intern(const std::string_view str);

  std::string_view get(StringId id) const { return pool_.get(id); }
  constexpr const StringPool& pool() const { return pool_; }

  // Returns the total size of all strings in the pool.
  usize size() const { return pool_.size(); }
  // Returns the number of strings in the pool.
  usize string_count() const { return pool_.string_count(); }

 private:
  // The pool comes first: the table appends to it, so it has to be constructed
  // before the table and destroyed after it.
  StringPool pool_;
  InternTable<> table_;
};

}  // namespace str
