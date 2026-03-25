// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/arena_deleter.h"
#include "base/debug.h"
#include "base/numeric.h"
#include "base/page_allocator.h"

namespace base {

class ArenaAllocator {
 public:
  // 2 MiB -> 1 huge page or 512 normal pages
  static constexpr usize kChunkSize = 2 * 1024 * 1024;

  // 1024 * 2 MiB = 2GiB
  static constexpr usize kMaxChunks = 1024;

  struct alignas(std::max_align_t) Chunk {
    usize capacity = 0;
    usize used = 0;

    // Can be written after the header.
    u8* data() { return reinterpret_cast<u8*>(this + 1); }
  };

  struct ChunkPosition {
    u32 chunk_id;
    usize offset;
  };

  ArenaAllocator() = default;
  ~ArenaAllocator() { reset(); }

  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;

  ArenaAllocator(ArenaAllocator&& other) noexcept;
  ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;

  void reserve(usize size, bool use_huge_pages = false);

  [[nodiscard]] void* alloc(usize size,
                            bool use_huge_pages = false,
                            usize align = alignof(std::max_align_t),
                            ChunkPosition* chunk_pos = nullptr);
  void reset();

  inline const Chunk* chunk(usize index) const {
    dcheck(index < chunk_count_);
    dcheck(chunks_);
    return chunks_[index];
  }

  inline const ChunkPosition current_position() const {
    if (chunks_) [[likely]] {
      return {
          .chunk_id = chunk_count_ - 1,
          .offset = chunks_[chunk_count_ - 1]->used,
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
  void alloc_new_chunk(usize size, bool use_huge_pages);
  void init_chunks_map(Chunk* first_chunk);
  void* try_alloc_from_chunk(usize size, usize align);
  void write_chunk_pos(void* ptr, ChunkPosition* pos);

  Chunk** chunks_ = nullptr;
  u32 chunk_count_ = 0;
};

}  // namespace base
