// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>

namespace io {

class TempFile {
 public:
  TempFile();
  explicit TempFile(std::string_view prefix);
  ~TempFile();

  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;

  TempFile(TempFile&& other) noexcept;
  TempFile& operator=(TempFile&& other) noexcept;

  // Explicitly deletes the temporary file before destruction.
  void remove();

  [[nodiscard]] std::string_view path() const { return path_; }
  [[nodiscard]] bool is_valid() const { return !path_.empty(); }

 private:
  bool create_temp_file(std::string_view prefix);

  std::string path_;
};

}  // namespace io
