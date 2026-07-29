// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/io/temp_file.h"

#include <string>
#include <string_view>
#include <utility>

#include "fpag/base/numeric.h"
#include "fpag/build/build_flag.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <stdlib.h>
#include <unistd.h>
#endif

namespace io {

TempFile::TempFile() {
  create_temp_file("temp_file_");
}

TempFile::TempFile(std::string_view prefix) {
  create_temp_file(prefix);
}

TempFile::~TempFile() {
  remove();
}

TempFile::TempFile(TempFile&& other) noexcept : path_(std::move(other.path_)) {
  other.path_.clear();
}

TempFile& TempFile::operator=(TempFile&& other) noexcept {
  if (this != &other) {
    remove();
    path_ = std::move(other.path_);
    other.path_.clear();
  }
  return *this;
}

void TempFile::remove() {
  if (path_.empty()) {
    return;
  }

#if FPAG_BUILD_FLAG(IS_OS_WIN)
  ::DeleteFileA(path_.c_str());
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  ::unlink(path_.c_str());
#endif

  path_.clear();
}

bool TempFile::create_temp_file(std::string_view prefix) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  char temp_path[MAX_PATH];
  DWORD path_len = ::GetTempPathA(MAX_PATH, temp_path);
  if (path_len == 0 || path_len > MAX_PATH) {
    return false;
  }

  char filename[MAX_PATH];
  std::string prefix_str(prefix);
  if (::GetTempFileNameA(temp_path, prefix_str.c_str(), 0, filename) == 0) {
    return false;
  }

  path_ = filename;
  return true;
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
  // Construct template for mkstemp: e.g. "/tmp/prefix_XXXXXX"
  std::string template_path = "/tmp/";
  template_path.append(prefix);
  template_path.append("XXXXXX");

  // mkstemp modifies the buffer in place.
  const i32 fd = ::mkstemp(template_path.data());
  if (fd < 0) {
    return false;
  }

  // Close descriptor immediately as we only need the allocated path.
  ::close(fd);

  path_ = std::move(template_path);
  return true;
#endif
}

}  // namespace io
