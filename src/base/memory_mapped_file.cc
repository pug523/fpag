// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/memory_mapped_file.h"

#include "fpag/base/file_handle.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_flag.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <sys/mman.h>
#include <sys/types.h>
#endif

namespace base {

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
    : data_(other.data_),
      size_(other.size_)
#if FPAG_BUILD_FLAG(IS_OS_WIN)
      ,
      mapping_handle_(other.mapping_handle_)
#endif
{
  other.data_ = nullptr;
  other.size_ = 0;
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  other.mapping_handle_ = nullptr;
#endif
}

MemoryMappedFile& MemoryMappedFile::operator=(
    MemoryMappedFile&& other) noexcept {
  if (this != &other) {
    close();
    data_ = other.data_;
    size_ = other.size_;
    other.data_ = nullptr;
    other.size_ = 0;
#if FPAG_BUILD_FLAG(IS_OS_WIN)
    mapping_handle_ = other.mapping_handle_;
    other.mapping_handle_ = nullptr;
#endif
  }
  return *this;
}

bool MemoryMappedFile::map(const FileHandle& file,
                           usize offset,
                           usize length,
                           bool populate_now) {
  close();

  if (!file.is_valid()) {
    return false;
  }

  const usize file_size = file.get_size();
  if (file_size == 0) {
    return false;
  }

  if (offset >= file_size) {
    return false;
  }

  if (length == 0) {
    size_ = file_size - offset;
  } else {
    if (offset + length < offset || (offset + length) > file_size) {
      return false;
    }
    size_ = length;
  }

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  const bool read_write = (file.access() == FileAccess::ReadWrite);
  const DWORD page_protect = read_write ? PAGE_READWRITE : PAGE_READONLY;
  const DWORD map_access = read_write ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

  ULARGE_INTEGER map_size;
  map_size.QuadPart = offset + size_;

  mapping_handle_ =
      ::CreateFileMappingA(file.native_handle(), nullptr, page_protect,
                           map_size.HighPart, map_size.LowPart, nullptr);
  if (!mapping_handle_) {
    return false;
  }

  ULARGE_INTEGER offset_li;
  offset_li.QuadPart = offset;

  data_ = static_cast<u8*>(::MapViewOfFile(mapping_handle_, map_access,
                                           offset_li.HighPart,
                                           offset_li.LowPart, size_));
  if (!data_) {
    ::CloseHandle(mapping_handle_);
    mapping_handle_ = nullptr;
    return false;
  }

  if (populate_now) {
    WIN32_MEMORY_RANGE_ENTRY entry;
    entry.VirtualAddress = data_;
    entry.NumberOfBytes = size_;
    ::PrefetchVirtualMemory(::GetCurrentProcess(), 1, &entry, 0);
  }
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  const i32 prot = (file.access() == FileAccess::ReadWrite)
                       ? (PROT_READ | PROT_WRITE)
                       : PROT_READ;

  i32 flags = MAP_SHARED;
#if defined(MAP_POPULATE)
  if (populate_now) {
    flags |= MAP_POPULATE;
  }
#else
  // To avoid unused parameter warnings
  (void)populate_now;
#endif

  void* ptr = ::mmap(nullptr, size_, prot, flags, file.native_handle(),
                     static_cast<off_t>(offset));
  if (ptr == MAP_FAILED) {
    data_ = nullptr;
    size_ = 0;
    return false;
  }

  data_ = static_cast<u8*>(ptr);
#endif

  return true;
}

void MemoryMappedFile::close() {
  if (!is_mapped()) {
    return;
  }

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  ::UnmapViewOfFile(data_);
  ::CloseHandle(mapping_handle_);
  mapping_handle_ = nullptr;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  ::munmap(data_, size_);
#endif

  data_ = nullptr;
  size_ = 0;
}

bool MemoryMappedFile::flush(bool synchronous) {
  if (!is_mapped()) {
    return false;
  }

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  (void)synchronous;
  return ::FlushViewOfFile(data_, size_) != 0;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  const i32 flags = synchronous ? MS_SYNC : MS_ASYNC;
  return ::msync(data_, size_, flags) == 0;
#else
  (void)synchronous;
#endif
}

void MemoryMappedFile::advise(AdviceHint hint) {
  if (!is_mapped()) {
    return;
  }

#if FPAG_BUILD_FLAG(IS_OS_POSIX)
  i32 native_hint = MADV_NORMAL;
  switch (hint) {
    case AdviceHint::Sequential: native_hint = MADV_SEQUENTIAL; break;
    case AdviceHint::Random: native_hint = MADV_RANDOM; break;
    case AdviceHint::WillNeed: native_hint = MADV_WILLNEED; break;
    default: break;
  }
  ::madvise(data_, size_, native_hint);
#else
  (void)hint;
#endif
}

}  // namespace base
