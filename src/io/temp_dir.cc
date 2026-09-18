// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/io/temp_dir.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fpag/base/numeric.h"
#include "fpag/build/build_flag.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace io {

TempDir::TempDir(std::string_view name) {
  std::string base;
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  char buf[1024];
  const DWORD len = ::GetTempPathA(sizeof(buf), buf);
  base = (len > 0 && len < sizeof(buf)) ? std::string(buf, len) : ".\\";
  for (char& c : base) {
    if (c == '\\') {
      c = '/';
    }
  }
#else
  const char* tmp = std::getenv("TMPDIR");
  base = (tmp != nullptr && tmp[0] != '\0') ? tmp : "/tmp";
  if (!base.empty() && base.back() != '/') {
    base.push_back('/');
  }
#endif
  path_ = base + std::string(name);
  remove_all(path_);
  make_dirs(path_);
}

TempDir::~TempDir() {
  remove();
}

TempDir::TempDir(TempDir&& other) noexcept
    : path_(std::move(other.path_)),
      files_(std::move(other.files_)),
      dirs_(std::move(other.dirs_)) {
  other.path_.clear();
}

TempDir& TempDir::operator=(TempDir&& other) noexcept {
  if (this != &other) {
    remove();
    path_ = std::move(other.path_);
    files_ = std::move(other.files_);
    dirs_ = std::move(other.dirs_);
    other.path_.clear();
  }
  return *this;
}

void TempDir::remove() {
  if (path_.empty()) {
    return;
  }
  for (usize i = files_.size(); i > 0; --i) {
    std::remove(files_[i - 1].c_str());
  }
  files_.clear();
  for (usize i = dirs_.size(); i > 0; --i) {
    remove_dir(dirs_[i - 1]);
  }
  dirs_.clear();
  remove_dir(path_);
  path_.clear();
}

std::string TempDir::join(std::string_view child) const {
  std::string out(path_);
  out.push_back('/');
  out.append(child);
  return out;
}

bool TempDir::make_dir(std::string_view rel) {
  const std::string full = join(rel);
  make_dirs(full);
  dirs_.push_back(full);
  return true;
}

bool TempDir::write_file(std::string_view rel, std::string_view bytes) {
  const std::string full = join(rel);
  const usize slash = full.find_last_of('/');
  if (slash != std::string::npos) {
    make_dirs(full.substr(0, slash));
  }
  std::FILE* file = std::fopen(full.c_str(), "wb");
  if (file == nullptr) {
    return false;
  }
  const usize written = std::fwrite(bytes.data(), 1, bytes.size(), file);
  std::fclose(file);
  if (written != bytes.size()) {
    return false;
  }
  files_.push_back(full);
  return true;
}

void TempDir::make_dirs(std::string_view path) {
  std::string current;
  usize index = 0;
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  // Preserve drive-letter roots like "C:".
  if (path.size() >= 2 && path[1] == ':') {
    current = std::string(path.substr(0, 2));
    index = 2;
  }
#endif
  for (; index <= path.size(); ++index) {
    if (index == path.size() || path[index] == '/') {
      if (!current.empty()) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
        ::_mkdir(current.c_str());
#else
        ::mkdir(current.c_str(), 0755);
#endif
      }
    }
    if (index < path.size()) {
      current.push_back(path[index]);
    }
  }
}

void TempDir::remove_all(std::string_view path) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  WIN32_FIND_DATAA found;
  const std::string pattern = std::string(path) + "/*";
  HANDLE handle = ::FindFirstFileA(pattern.c_str(), &found);
  if (handle == INVALID_HANDLE_VALUE) {
    return;
  }
  do {
    const std::string_view name(found.cFileName);
    if (name == "." || name == "..") {
      continue;
    }
    const std::string full = std::string(path) + "/" + std::string(name);
    if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
      remove_all(full);
      ::_rmdir(full.c_str());
    } else {
      std::remove(full.c_str());
    }
  } while (::FindNextFileA(handle, &found) != 0);
  ::FindClose(handle);
#else
  DIR* dir = ::opendir(std::string(path).c_str());
  if (dir == nullptr) {
    return;
  }
  while (dirent* entry = ::readdir(dir)) {
    const std::string_view name(entry->d_name);
    if (name == "." || name == "..") {
      continue;
    }
    const std::string full = std::string(path) + "/" + std::string(name);
    struct stat info;
    // lstat: never follow symlinks, so link cycles are impossible.
    if (::lstat(full.c_str(), &info) != 0) {
      continue;
    }
    if (S_ISDIR(info.st_mode)) {
      remove_all(full);
      ::rmdir(full.c_str());
    } else {
      std::remove(full.c_str());
    }
  }
  ::closedir(dir);
#endif
}

void TempDir::remove_dir(std::string_view path) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  ::_rmdir(std::string(path).c_str());
#else
  ::rmdir(std::string(path).c_str());
#endif
}

}  // namespace io
