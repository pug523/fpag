// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <string_view>

#include "fpag/base/numeric.h"
#include "fpag/io/file_handle.h"
#include "fpag/io/memory_mapped_file.h"

namespace io {

class MemoryMappedStreamWriter {
 public:
  MemoryMappedStreamWriter() = default;
  ~MemoryMappedStreamWriter() { finish(); }

  MemoryMappedStreamWriter(const MemoryMappedStreamWriter&) = delete;
  MemoryMappedStreamWriter& operator=(const MemoryMappedStreamWriter&) = delete;

  MemoryMappedStreamWriter(MemoryMappedStreamWriter&&) noexcept = default;
  MemoryMappedStreamWriter& operator=(MemoryMappedStreamWriter&&) noexcept =
      default;

  // Initializes file and maps initial space.
  bool open(std::string_view path,
            usize initial_capacity = static_cast<usize>(1024 * 1024)) {
    if (!file_.open(path, FileAccess::ReadWrite)) {
      return false;
    }
    capacity_ = initial_capacity;
    if (!file_.resize(capacity_)) {
      file_.close();
      return false;
    }
    if (!mmap_.map(file_, 0, capacity_, true)) {
      file_.close();
      return false;
    }
    mmap_.advise(AdviceHint::Sequential);
    write_offset_ = 0;
    return true;
  }

  // Ensures capacity for at least `required_bytes`.
  bool reserve_extra(usize required_bytes) {
    if (write_offset_ + required_bytes <= capacity_) {
      return true;
    }

    usize new_capacity = capacity_ * 2;
    while (write_offset_ + required_bytes > new_capacity) {
      new_capacity *= 2;
    }

    mmap_.close();
    if (!file_.resize(new_capacity)) {
      return false;
    }
    if (!mmap_.map(file_, 0, new_capacity, true)) {
      return false;
    }
    mmap_.advise(AdviceHint::Sequential);
    capacity_ = new_capacity;
    return true;
  }

  // Appends bytes directly to the mapped region.
  bool write(const void* data, usize length) {
    if (!reserve_extra(length)) {
      return false;
    }
    std::memcpy(mmap_.data() + write_offset_, data, length);
    write_offset_ += length;
    return true;
  }

  // Returns pointer to current write head with guaranteed write size available.
  u8* prepare_write_buffer(usize length) {
    if (!reserve_extra(length)) {
      return nullptr;
    }
    return mmap_.data() + write_offset_;
  }

  // Advances written byte size after writing directly into buffer from
  // prepare_write_buffer.
  void commit_write(usize length) { write_offset_ += length; }

  // Truncates file to exact written size and unmaps.
  void finish() {
    if (!file_.is_valid()) {
      return;
    }
    mmap_.flush(/*synchronous=*/false);
    mmap_.close();
    file_.resize(write_offset_);
    file_.close();
  }

  [[nodiscard]] usize bytes_written() const { return write_offset_; }

 private:
  FileHandle file_;
  MemoryMappedFile mmap_;
  usize capacity_ = 0;
  usize write_offset_ = 0;
};

}  // namespace io
