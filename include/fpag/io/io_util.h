// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cerrno>
#include <cstddef>
#include <string>

#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <io.h>
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <unistd.h>
#else
#error "Unsupported platform"
#endif

namespace io {

bool is_file(const std::string& file_name);
bool is_dir(const std::string& dir_name);

i32 open(const std::string& path);
void close(i32 fd);

bool create_file(const std::string& path, bool exist_ok = true);
bool create_directory(const std::string& path, bool exist_ok = true);

bool remove_file(const std::string& path);
bool remove_directory(const std::string& path);

bool rename_file(const std::string& old_path, const std::string& new_path);

#if FPAG_BUILD_FLAG(IS_OS_WIN)
constexpr i32 kStdoutFd = 1;
constexpr i32 kStderrFd = 2;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
constexpr i32 kStdoutFd = STDOUT_FILENO;
constexpr i32 kStderrFd = STDERR_FILENO;
#endif

inline void write(i32 fd, const char* data, usize size) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  ::_write(fd, data, static_cast<u32>(size));
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  [[maybe_unused]] const isize _ = ::write(fd, data, size);
#endif
}

}  // namespace io
