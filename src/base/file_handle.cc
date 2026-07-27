// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/file_handle.h"

#include <stdio.h>

#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/build/build_flag.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace base {

FileHandle::FileHandle(FileHandle&& other) noexcept
    : handle_(other.handle_), access_(other.access_) {
  other.handle_ = kInvalidHandle;
}

FileHandle& FileHandle::operator=(FileHandle&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = other.handle_;
    access_ = other.access_;
    other.handle_ = kInvalidHandle;
  }
  return *this;
}

bool FileHandle::open(std::string_view path, FileAccess access) {
  close();
  access_ = access;

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  DWORD desired_access = GENERIC_READ;
  DWORD share_mode = FILE_SHARE_READ;
  DWORD creation_disposition = OPEN_EXISTING;

  if (access == FileAccess::ReadWrite) {
    desired_access |= GENERIC_WRITE;
    creation_disposition = OPEN_ALWAYS;
  }

  handle_ = ::CreateFileA(path.data(), desired_access, share_mode, nullptr,
                          creation_disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
  return handle_ != INVALID_HANDLE_VALUE;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  const i32 flags =
      (access == FileAccess::ReadWrite) ? O_RDWR | O_CREAT : O_RDONLY;
  handle_ = ::open(path.data(), flags, 0644);
  return handle_ >= 0;
#endif
}

void FileHandle::close() {
  if (!is_valid()) {
    return;
  }
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  ::CloseHandle(handle_);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  ::close(handle_);
#endif
  handle_ = kInvalidHandle;
}

bool FileHandle::resize(usize new_size) {
  if (!is_valid() || access_ != FileAccess::ReadWrite) {
    return false;
  }

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  LARGE_INTEGER li;
  li.QuadPart = static_cast<LONGLONG>(new_size);
  if (!::SetFilePointerEx(handle_, li, nullptr, FILE_BEGIN) ||
      !::SetEndOfFile(handle_)) {
    return false;
  }
  return true;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return ::ftruncate(handle_, static_cast<off_t>(new_size)) == 0;
#endif
}

usize FileHandle::get_size() const {
  if (!is_valid()) {
    return 0;
  }

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  LARGE_INTEGER file_size;
  if (!::GetFileSizeEx(handle_, &file_size)) {
    return 0;
  }
  return static_cast<usize>(file_size.QuadPart);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  struct stat st;
  if (::fstat(handle_, &st) < 0) {
    return 0;
  }
  return static_cast<usize>(st.st_size);
#endif
}

}  // namespace base
