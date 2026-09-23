// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace io {

// RAII scratch directory for tests, built on C/POSIX/Win32 APIs.
// Best-effort cleanup.
//
// The directory name is fixed (not uniquified): construction wipes any
// previous contents, so parallel tests must use distinct names. Children
// canonically join with '/', which Windows file APIs accept, keeping
// expectations identical across platforms.
class TempDir {
 public:
  explicit TempDir(std::string_view name);
  ~TempDir();

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  TempDir(TempDir&& other) noexcept;
  TempDir& operator=(TempDir&& other) noexcept;

  // Recursively deletes the directory before destruction.
  void remove();

  std::string_view path() const { return path_; }
  bool is_valid() const { return !path_.empty(); }

  std::string join(std::string_view child) const;
  bool make_dir(std::string_view rel);
  bool write_file(std::string_view rel, std::string_view bytes);

 private:
  static void make_dirs(std::string_view path);
  static void remove_all(std::string_view path);
  static void remove_dir(std::string_view path);

  std::string path_;
  std::vector<std::string> files_;
  std::vector<std::string> dirs_;
};

}  // namespace io
