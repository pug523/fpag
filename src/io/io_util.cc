// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/io/io_util.h"

#include <cerrno>
#include <cstdio>
#include <span>
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

namespace {

class ScopedFd {
 public:
  explicit ScopedFd(i32 fd = -1) : fd_(fd) {}
  ~ScopedFd() { reset(); }

  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;

  ScopedFd(ScopedFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

  ScopedFd& operator=(ScopedFd&& other) noexcept {
    if (this != &other) {
      reset();
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  void reset(i32 new_fd = -1) {
    if (fd_ != -1) {
      close(fd_);
    }
    fd_ = new_fd;
  }

  [[nodiscard]] i32 get() const { return fd_; }
  [[nodiscard]] bool is_valid() const { return fd_ != -1; }

 private:
  i32 fd_ = -1;
};

}  // namespace

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

i32 open(const std::string& path, i32 flags, i32 mode) {
  const char* ptr = path.c_str();
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  (void)mode;
  return ::_open(ptr, flags);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  return ::open(ptr, flags, mode);
#endif
}

i32 open(const std::string& path, i32 flags) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  constexpr i32 kDefaultMode = _S_IREAD | _S_IWRITE;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  constexpr i32 kDefaultMode = 0644;
#endif
  return open(path, flags, kDefaultMode);
}

i32 open(const std::string& path) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  constexpr i32 kDefaultFlags = _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  constexpr i32 kDefaultFlags = O_WRONLY | O_CREAT | O_TRUNC;
#endif
  return open(path, kDefaultFlags);
}

void close(i32 fd) {
  if (fd < 0) {
    return;
  }
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  ::_close(fd);
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  ::close(fd);
#endif
}

bool create_file(const std::string& path, bool exist_ok) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  i32 flags = _O_WRONLY | _O_CREAT | _O_BINARY;
  if (!exist_ok) {
    flags |= _O_EXCL;
  } else {
    flags |= _O_TRUNC;
  }
  ScopedFd fd(open(path, flags, _S_IREAD | _S_IWRITE));
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  i32 flags = O_WRONLY | O_CREAT;
  if (!exist_ok) {
    flags |= O_EXCL;
  } else {
    flags |= O_TRUNC;
  }
  const ScopedFd fd(open(path, flags, 0644));
#endif

  return fd.is_valid();
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

isize file_size(const std::string& path) {
  if (!is_file(path)) {
    return -1;
  }
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  const ScopedFd fd(open(path, _O_RDONLY | _O_BINARY));
  if (!fd.is_valid()) {
    return -1;
  }
  struct _stat64i32 st;
  if (_fstat(fd.get(), &st) != 0) {
    return -1;
  }
#else
  const ScopedFd fd(open(path, O_RDONLY));
  if (!fd.is_valid()) {
    return -1;
  }
  struct stat st;
  if (fstat(fd.get(), &st) != 0) {
    return -1;
  }
#endif
  return st.st_size;
}

std::string read_file(const std::string& path) {
  const isize size = file_size(path);
  if (size <= 0) {
    return "";
  }
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  const ScopedFd fd(open(path, _O_RDONLY | _O_BINARY));
#else
  const ScopedFd fd(open(path, O_RDONLY));
#endif
  if (!fd.is_valid()) {
    return "";
  }

  std::string result;
  result.resize(static_cast<usize>(size));
  isize total_read = 0;
  while (total_read < size) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
    i64 bytes = _read(fd.get(), &result[static_cast<usize>(total_read)],
                      static_cast<u32>(size - total_read));
#else
    const isize bytes =
        ::read(fd.get(), &result[static_cast<usize>(total_read)],
               static_cast<usize>(size - total_read));
#endif
    if (bytes <= 0) {
      break;
    }
    total_read += static_cast<usize>(bytes);
  }
  result.resize(static_cast<usize>(total_read));

  return result;
}

bool write_file(const std::span<const u8> data,
                const std::string& output_path) {
  if (data.empty()) {
    return false;
  }

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  constexpr i32 kFlags = _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY;
  constexpr i32 kMode = _S_IREAD | _S_IWRITE;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  constexpr i32 kFlags = O_WRONLY | O_CREAT | O_TRUNC;
  constexpr i32 kMode = 0644;
#endif

  const ScopedFd fd(open(output_path, kFlags, kMode));
  if (!fd.is_valid()) {
    return false;
  }

  const u8* ptr = data.data();
  usize remaining = data.size();

  while (remaining > 0) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
    const i32 bytes = _write(fd.get(), ptr, static_cast<u32>(remaining));
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
    const isize bytes = ::write(fd.get(), ptr, remaining);
#endif

    if (bytes <= 0) {
      return false;
    }

    ptr += bytes;
    remaining -= static_cast<usize>(bytes);
  }

  return true;
}

}  // namespace io
