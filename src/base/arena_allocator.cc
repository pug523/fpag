// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/arena_allocator.h"

#include <algorithm>

#include "base/math_util.h"
#include "base/numeric.h"

namespace base {

ArenaAllocator::ArenaAllocator(ArenaAllocator&& other) noexcept
    : chunks_(other.chunks_), chunk_count_(other.chunk_count_) {
  other.chunks_ = nullptr;
  other.chunk_count_ = 0;
}

ArenaAllocator& ArenaAllocator::operator=(ArenaAllocator&& other) noexcept {
  if (this != &other) {
    reset();
    chunks_ = other.chunks_;
    chunk_count_ = other.chunk_count_;
    other.chunks_ = nullptr;
    other.chunk_count_ = 0;
  }
  return *this;
}

void ArenaAllocator::reserve(usize size, bool use_huge_pages) {
  // Simply push a new chunk; no need to check for sufficient space.
  alloc_new_chunk(size, use_huge_pages);
}

void* ArenaAllocator::alloc(usize size,
                            bool use_huge_pages,
                            usize align,
                            ChunkPosition* chunk_pos) {
  dcheck(is_power_of_two(align));

  // First, try to allocate from the current chunks if exists.
  if (chunks_ && chunk_count_ > 0) {
    void* ptr = try_alloc_from_chunk(size, align);
    if (ptr) {
      if (chunk_pos) {
        write_chunk_pos(ptr, chunk_pos);
      }
      return ptr;
    }
  }

  // If no chunks exist or the last chunk is full, allocate a new one.
  usize required_data_size = size + align;
  if (!chunks_) [[unlikely]] {
    required_data_size += sizeof(Chunk*) * kMaxChunks;
  }

  alloc_new_chunk(required_data_size, use_huge_pages);

  // Try to allocate from the new chunk.
  void* ptr = try_alloc_from_chunk(size, align);
  dcheck(ptr);
  if (chunk_pos) {
    write_chunk_pos(ptr, chunk_pos);
  }
  return ptr;
}

void ArenaAllocator::reset() {
  if (!chunks_) [[unlikely]] {
    return;
  }

  Chunk** chunks_ptr = chunks_;
  usize count = chunk_count_;

  for (usize i = 1; i < count; ++i) {
    if (chunks_ptr[i]) [[likely]] {
      free_pages(chunks_ptr[i], sizeof(Chunk) + chunks_ptr[i]->capacity);
    }
  }
  // Free the first chunk separately which contains the `chunks_` directly.
  if (count > 0 && chunks_ptr[0]) {
    free_pages(chunks_[0], sizeof(Chunk) + chunks_[0]->capacity);
  }
  chunks_ = nullptr;
  chunk_count_ = 0;
}

void ArenaAllocator::alloc_new_chunk(usize size, bool use_huge_pages) {
  dcheck_msg(chunk_count_ < kMaxChunks,
             "ArenaAllocator out of memory (too many chunks)");

  // Consider header size, ensure at least kChunkSize is allocated
  const usize min_required = size + sizeof(Chunk);
  const usize alloc_size = std::max(kChunkSize, min_required);

  void* mem = use_huge_pages ? allocate_huge_pages(alloc_size)
                             : allocate_pages(alloc_size);
  dcheck(mem);

  Chunk* new_chunk = static_cast<Chunk*>(mem);
  new_chunk->capacity = alloc_size - sizeof(Chunk);
  new_chunk->used = 0;

  if (!chunks_) [[unlikely]] {
    init_chunks_map(new_chunk);
  }

  chunks_[chunk_count_++] = new_chunk;
}

void ArenaAllocator::init_chunks_map(Chunk* first_chunk) {
  dcheck(first_chunk);
  chunks_ = reinterpret_cast<Chunk**>(first_chunk->data());

  const usize map_size = sizeof(Chunk*) * kMaxChunks;
  dcheck(map_size <= first_chunk->capacity);
  first_chunk->used = map_size;
}

void* ArenaAllocator::try_alloc_from_chunk(usize size, usize align) {
  if (!chunks_ || chunk_count_ == 0) [[unlikely]] {
    return nullptr;
  }

  Chunk* const chunk = chunks_[chunk_count_ - 1];

  // Current write position.
  u8* const curr_ptr = chunk->data() + chunk->used;
  const uintptr_t curr_addr = reinterpret_cast<uintptr_t>(curr_ptr);

  // Calculate the aligned address.
  const uintptr_t aligned_addr =
      (curr_addr + align - 1) & ~(uintptr_t(align) - 1);
  const usize padding = static_cast<usize>(aligned_addr - curr_addr);

  // Check if padding + size fits within the chunk's capacity.
  if (chunk->used + padding + size <= chunk->capacity) {
    chunk->used += padding + size;
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    return reinterpret_cast<void*>(aligned_addr);
  }

  // No space available.
  return nullptr;
}

inline void ArenaAllocator::write_chunk_pos(void* ptr, ChunkPosition* pos) {
  dcheck(ptr);
  dcheck(pos);
  pos->chunk_id = chunk_count_ - 1;
  pos->offset = static_cast<usize>(
      static_cast<u8*>(ptr) - reinterpret_cast<u8*>(chunks_[chunk_count_ - 1]));
}

}  // namespace base
