// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cerrno>
#include <cstddef>
#include <span>
#include <string>

#include "fpag/base/numeric.h"

namespace io {

bool is_file(const std::string& file_name);
bool is_dir(const std::string& dir_name);

i32 open(const std::string& path, i32 flags, i32 mode);
i32 open(const std::string& path, i32 flags);
i32 open(const std::string& path);
void close(i32 fd);

bool create_file(const std::string& path, bool exist_ok = true);
bool create_directory(const std::string& path, bool exist_ok = true);

bool remove_file(const std::string& path);
bool remove_directory(const std::string& path);

bool rename_file(const std::string& old_path, const std::string& new_path);

isize file_size(const std::string& path);
std::string read_file(const std::string& path);

bool write_file(const std::span<const u8> data, const std::string& output_path);

// File descriptors are 1 and 2 on every platform fpag supports, so this does
// not need a platform branch and the header does not need a platform include.
constexpr i32 STDOUT_FD = 1;
constexpr i32 STDERR_FD = 2;

void write(i32 fd, const char* data, usize size);

}  // namespace io
