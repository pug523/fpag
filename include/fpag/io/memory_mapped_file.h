// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <span>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/io/file_handle.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#endif

namespace io {

enum class AdviceHint : u8 {
  Normal,
  Sequential,
  Random,
  WillNeed,
};

class MemoryMappedFile {
 public:
  MemoryMappedFile() = default;
  ~MemoryMappedFile() { close(); }

  MemoryMappedFile(const MemoryMappedFile&) = delete;
  MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;

  MemoryMappedFile(MemoryMappedFile&& other) noexcept;
  MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;

  // Maps an existing file handle into virtual memory.
  // Set populate_now to true to force kernel page-table pre-population.
  bool map(const FileHandle& file,
           usize offset = 0,
           usize length = 0,
           bool populate_now = false);

  void close();

  // Flushes modified pages back to storage asynchronously or synchronously.
  bool flush(bool synchronous = false);

  // Provides hints to kernel page management (e.g., MADV_SEQUENTIAL).
  void advise(AdviceHint hint);

  [[nodiscard]] constexpr u8* data() { return data_; }
  [[nodiscard]] constexpr const u8* data() const { return data_; }
  [[nodiscard]] constexpr usize size() const { return size_; }
  [[nodiscard]] constexpr bool is_mapped() const { return data_ != nullptr; }

  [[nodiscard]] constexpr std::span<u8> as_span() { return {data_, size_}; }
  [[nodiscard]] constexpr std::span<const u8> as_span() const {
    return {data_, size_};
  }

 private:
  u8* data_ = nullptr;
  usize size_ = 0;

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  HANDLE mapping_handle_ = nullptr;
#endif
};

}  // namespace io
