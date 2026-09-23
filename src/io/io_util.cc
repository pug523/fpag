// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/io/io_util.h"

#include <cerrno>
#include <cstdio>
#include <string>

#include "fpag/base/numeric.h"
#include "fpag/build/build_flag.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#error "Unsupported platform"
#endif

#include <fcntl.h>
#include <sys/stat.h>

namespace io {

bool is_file(const std::string& file_name) {
  const char* ptr = file_name.c_str();
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  DWORD attributes = GetFileAttributesA(ptr);
  return (attributes != INVALID_FILE_ATTRIBUTES &&
          !(attributes & FILE_ATTRIBUTE_DIRECTORY));
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  struct stat buffer;
  return (::stat(ptr, &buffer) == 0) && S_ISREG(buffer.st_mode);
#endif
}

bool is_dir(const std::string& dir_name) {
  const char* ptr = dir_name.c_str();
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  DWORD attributes = GetFileAttributesA(ptr);
  return (attributes != INVALID_FILE_ATTRIBUTES &&
          (attributes & FILE_ATTRIBUTE_DIRECTORY));
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  struct stat buffer;
  return (::stat(ptr, &buffer) == 0) && S_ISDIR(buffer.st_mode);
#endif
}

// Creates file if not exist (Truncates if exists)
i32 open(const std::string& path) {
  const char* ptr = path.c_str();
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  i32 flags = _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE;
  return ::_open(ptr, flags);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return ::open(ptr, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
}

void close(i32 fd) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  ::_close(fd);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  ::close(fd);
#endif
}

bool create_file(const std::string& path, bool exist_ok) {
  const char* ptr = path.c_str();

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  i32 flags = _O_WRONLY | _O_CREAT | _O_BINARY;
  if (!exist_ok) {
    flags |= _O_EXCL;
  } else {
    flags |= _O_TRUNC;
  }
  const i32 fd = ::_open(ptr, flags, _S_IREAD | _S_IWRITE);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  i32 flags = O_WRONLY | O_CREAT;
  if (!exist_ok) {
    flags |= O_EXCL;
  } else {
    flags |= O_TRUNC;
  }
  const i32 fd = ::open(ptr, flags, 0644);
#endif

  if (fd == -1) {
    return false;
  }
  close(fd);
  return true;
}

bool create_directory(const std::string& path, bool exist_ok) {
  const char* ptr = path.c_str();
  i32 res = -1;

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  res = ::_mkdir(ptr);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  res = ::mkdir(ptr, 0755);
#endif

  if (res == 0) {
    return true;
  }

  if (exist_ok && errno == EEXIST) {
    return is_dir(path);
  }

  return false;
}

bool remove_file(const std::string& path) {
  const char* ptr = path.c_str();
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  return DeleteFileA(ptr) != 0;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return ::unlink(ptr) == 0;
#endif
}

bool remove_directory(const std::string& path) {
  const char* ptr = path.c_str();
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  return RemoveDirectoryA(ptr) != 0;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return ::rmdir(ptr) == 0;
#endif
}

bool rename_file(const std::string& old_path, const std::string& new_path) {
  const char* old_ptr = old_path.c_str();
  const char* new_ptr = new_path.c_str();
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  return MoveFileA(old_ptr, new_ptr) != 0;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return ::rename(old_ptr, new_ptr) == 0;
#endif
}

}  // namespace io
