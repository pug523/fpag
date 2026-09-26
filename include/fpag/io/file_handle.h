// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

namespace io {

enum class FileAccess : u8 {
  Read,
  ReadWrite,
};

class FileHandle {
 public:
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  // HANDLE and INVALID_HANDLE_VALUE, spelled out. HANDLE is void*, so a
  // consumer sees the same type it would from <windows.h>, without the
  // SDK macro surface arriving through this header.
  using NativeHandle = void*;
  static inline const NativeHandle kInvalidHandle =
      reinterpret_cast<NativeHandle>(static_cast<isize>(-1));
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  using NativeHandle = i32;
  static inline const NativeHandle kInvalidHandle = -1;
#endif

  FileHandle() = default;
  ~FileHandle() { close(); }

  FileHandle(const FileHandle&) = delete;
  FileHandle& operator=(const FileHandle&) = delete;

  FileHandle(FileHandle&& other) noexcept;
  FileHandle& operator=(FileHandle&& other) noexcept;

  /// @brief Opens file using direct OS API without heap allocation for path.
  /// @note Path MUST be null-terminated for system calls.
  bool open(std::string_view path, FileAccess access);
  void close();

  bool resize(usize new_size);
  usize get_size() const;

  [[nodiscard]] bool is_valid() const { return handle_ != kInvalidHandle; }
  [[nodiscard]] constexpr NativeHandle native_handle() const { return handle_; }
  [[nodiscard]] constexpr FileAccess access() const { return access_; }

 private:
  NativeHandle handle_ = kInvalidHandle;
  FileAccess access_ = FileAccess::Read;
};

}  // namespace io
