// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/mem/arena_allocator.h"

#include <algorithm>

#include "base/math_util.h"
#include "base/numeric.h"

namespace base {

ArenaAllocator::ArenaAllocator(ArenaAllocator&& other) noexcept
    : block_(other.block_), block_count_(other.block_count_) {
  other.block_ = nullptr;
  other.block_count_ = 0;
}

ArenaAllocator& ArenaAllocator::operator=(ArenaAllocator&& other) noexcept {
  if (this != &other) {
    reset();
    block_ = other.block_;
    block_count_ = other.block_count_;
    other.block_ = nullptr;
    other.block_count_ = 0;
  }
  return *this;
}

void ArenaAllocator::reserve(usize size, bool use_huge_pages) {
  // Simply push a new block; no need to check for sufficient space.
  dcheck_gt(size, usize(0));
  alloc_new_block(size, use_huge_pages);
}

void* ArenaAllocator::alloc(usize size,
                            bool use_huge_pages,
                            usize align,
                            BlockPosition* block_pos) {
  dcheck(is_power_of_two(align));
  dcheck_gt(size, usize(0));

  // First, try to allocate from the current blocks if exists.
  if (block_ && block_count_ > 0) [[likely]] {
    void* ptr = try_alloc_from_block(size, align);
    if (ptr) {
      if (block_pos) {
        write_block_pos(ptr, block_pos);
      }
      return ptr;
    }
  }

  // If no blocks exist or the last block is full, allocate a new one.
  usize required_data_size = size + align;
  if (!block_) [[unlikely]] {
    required_data_size += sizeof(Block*) * kMaxBlocks;
  }

  alloc_new_block(required_data_size, use_huge_pages);

  // Try to allocate from the new block.
  void* ptr = try_alloc_from_block(size, align);
  dcheck(ptr);
  if (block_pos) {
    write_block_pos(ptr, block_pos);
  }
  return ptr;
}

void ArenaAllocator::reset() {
  if (!block_) [[unlikely]] {
    return;
  }

  Block** blocks_ptr = block_;
  usize count = block_count_;

  for (usize i = 1; i < count; ++i) {
    if (blocks_ptr[i]) [[likely]] {
      free_pages(blocks_ptr[i], sizeof(Block) + blocks_ptr[i]->capacity);
    }
  }
  // Free the first block separately which contains the `blocks_` directly.
  if (count > 0 && blocks_ptr[0]) {
    free_pages(block_[0], sizeof(Block) + block_[0]->capacity);
  }
  block_ = nullptr;
  block_count_ = 0;
}

void ArenaAllocator::alloc_new_block(usize size, bool use_huge_pages) {
  dcheck_msg(block_count_ < kMaxBlocks,
             "ArenaAllocator out of memory (too many blocks)");

  // Consider header size, ensure at least `kBlockSize` is allocated
  const usize min_required = size + sizeof(Block);
  const usize alloc_size = std::max(kBlockSize, min_required);

  void* mem = use_huge_pages ? allocate_huge_pages(alloc_size)
                             : allocate_pages(alloc_size);
  dcheck(mem);

  Block* new_block = static_cast<Block*>(mem);
  new_block->capacity = alloc_size - sizeof(Block);
  new_block->used = 0;

  if (!block_) [[unlikely]] {
    init_block_map(new_block);
  }

  block_[block_count_++] = new_block;
}

void ArenaAllocator::init_block_map(Block* first_block) {
  dcheck(first_block);
  block_ = reinterpret_cast<Block**>(first_block->data());

  const usize map_size = sizeof(Block*) * kMaxBlocks;
  dcheck_le(map_size, first_block->capacity);
  first_block->used = map_size;
}

void* ArenaAllocator::try_alloc_from_block(usize size, usize align) {
  if (!block_ || block_count_ == 0) [[unlikely]] {
    return nullptr;
  }

  Block* const block = block_[block_count_ - 1];

  // Current write position.
  u8* const curr_ptr = block->data() + block->used;
  const uintptr_t curr_addr = reinterpret_cast<uintptr_t>(curr_ptr);

  // Calculate the aligned address.
  const uintptr_t aligned_addr =
      (curr_addr + align - 1) & ~(uintptr_t(align) - 1);
  const usize padding = static_cast<usize>(aligned_addr - curr_addr);

  // Check if padding + size fits within the block's capacity.
  if (block->used + padding + size <= block->capacity) {
    block->used += padding + size;
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    return reinterpret_cast<void*>(aligned_addr);
  }

  // No space available.
  return nullptr;
}

inline void ArenaAllocator::write_block_pos(void* ptr, BlockPosition* pos) {
  dcheck(ptr);
  dcheck(pos);
  pos->block_id = block_count_ - 1;
  pos->offset = static_cast<usize>(
      static_cast<u8*>(ptr) - reinterpret_cast<u8*>(block_[block_count_ - 1]));
}

}  // namespace base
