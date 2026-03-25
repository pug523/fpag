// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/arena_deleter.h"
#include "base/debug/check.h"
#include "base/numeric.h"
#include "base/page_allocator.h"

namespace base {

class ArenaAllocator {
 public:
  // 2 MiB -> 1 huge page or 512 normal pages
  static constexpr usize kBlockSize = 2 * 1024 * 1024;

  // 2 MiB * 1024 blocks = 2GiB
  static constexpr usize kMaxBlocks = 1024;

  struct alignas(std::max_align_t) Block {
    usize capacity = 0;
    usize used = 0;

    // Can be written after the header.
    u8* data() { return reinterpret_cast<u8*>(this + 1); }
  };

  struct BlockPosition {
    u32 block_id;
    usize offset;
  };

  ArenaAllocator() = default;
  ~ArenaAllocator() { reset(); }

  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;

  ArenaAllocator(ArenaAllocator&& other) noexcept;
  ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;

  void reserve(usize size, bool use_huge_pages = true);

  [[nodiscard]] void* alloc(usize size,
                            bool use_huge_pages = true,
                            usize align = alignof(std::max_align_t),
                            BlockPosition* block_pos = nullptr);
  void reset();

  inline const Block* block(usize index) const {
    dcheck(index < block_count_);
    dcheck(block_);
    return block_[index];
  }

  inline const BlockPosition current_position() const {
    if (block_) [[likely]] {
      return {
          .block_id = block_count_ - 1,
          .offset = block_[block_count_ - 1]->used,
      };
    } else {
      return {0, 0};
    }
  }

  // Doesn't call destructor.
  // Use this for trivial copyable types.
  template <typename T, typename... Args>
  [[nodiscard]] inline T* create(Args&&... args) {
    void* mem = alloc(sizeof(T), alignof(T));
    T* const obj = new (mem) T(std::forward<Args>(args)...);
    dcheck(obj);
    return obj;
  }

  // Calls destructor automatically.
  template <typename T, typename... Args>
  [[nodiscard]] inline ArenaUniquePtr<T> create_managed(Args&&... args) {
    return ArenaUniquePtr<T>(create<T>(std::forward<Args>(args)...));
  }

 private:
  void alloc_new_block(usize size, bool use_huge_pages);
  void init_block_map(Block* first_block);
  void* try_alloc_from_block(usize size, usize align);
  void write_block_pos(void* ptr, BlockPosition* pos);

  Block** block_ = nullptr;
  u32 block_count_ = 0;
};

}  // namespace base
