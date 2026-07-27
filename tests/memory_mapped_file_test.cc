// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/memory_mapped_file.h"

#include <cstring>
#include <string_view>
#include <utility>
#include <span>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/file_handle.h"
#include "fpag/base/numeric.h"
#include "fpag/base/temp_file.h"

namespace base {

namespace {

constexpr std::string_view kTestContent = "Hello, MemoryMappedFile Test!";

}  // namespace

TEST_CASE("FileHandle basic lifetime and resizing", "[base][file_handle]") {
  TempFile const temp_file;
  REQUIRE(temp_file.is_valid());

  FileHandle handle;
  REQUIRE_FALSE(handle.is_valid());

  SECTION("Open non-existent file for read fails") {
    FileHandle invalid_handle;
    CHECK_FALSE(invalid_handle.open("non_existent_file_path_12345.bin",
                                    FileAccess::Read));
    CHECK_FALSE(invalid_handle.is_valid());
  }

  SECTION("Open and resize file") {
    REQUIRE(handle.open(temp_file.path(), FileAccess::ReadWrite));
    CHECK(handle.is_valid());

    constexpr usize kTargetSize = 4096;
    REQUIRE(handle.resize(kTargetSize));
    CHECK(handle.get_size() == kTargetSize);
  }

  SECTION("Move semantics for FileHandle") {
    REQUIRE(handle.open(temp_file.path(), FileAccess::ReadWrite));
    FileHandle moved_handle = std::move(handle);

    CHECK_FALSE(handle.is_valid());
    CHECK(moved_handle.is_valid());

    moved_handle.close();
    CHECK_FALSE(moved_handle.is_valid());
  }
}

TEST_CASE("MemoryMappedFile write and read roundtrip",
          "[base][memory_mapped_file]") {
  TempFile const temp_file;
  REQUIRE(temp_file.is_valid());

  constexpr usize kFileSize = 1024;

  SECTION("Write content via mmap and verify read via mmap") {
    // Write Phase
    {
      FileHandle file;
      REQUIRE(file.open(temp_file.path(), FileAccess::ReadWrite));
      REQUIRE(file.resize(kFileSize));

      MemoryMappedFile mmap_writer;
      REQUIRE(mmap_writer.map(file, 0, kFileSize, /*populate_now=*/true));
      CHECK(mmap_writer.is_mapped());
      CHECK(mmap_writer.size() == kFileSize);

      mmap_writer.advise(AdviceHint::Sequential);

      std::span<u8> const buffer = mmap_writer.as_span();
      std::memcpy(buffer.data(), kTestContent.data(), kTestContent.size());

      CHECK(mmap_writer.flush(/*synchronous=*/true));
    }

    // Read Phase
    {
      FileHandle file;
      REQUIRE(file.open(temp_file.path(), FileAccess::Read));

      MemoryMappedFile mmap_reader;
      REQUIRE(mmap_reader.map(file, 0, 0, /*populate_now=*/true));
      CHECK(mmap_reader.is_mapped());
      CHECK(mmap_reader.size() == kFileSize);

      std::span<const u8> const read_buffer = mmap_reader.as_span();
      std::string_view read_text(
          reinterpret_cast<const char*>(read_buffer.data()),
          kTestContent.size());

      CHECK(read_text == kTestContent);
    }
  }
}

TEST_CASE("MemoryMappedFile move semantics and resource cleanup",
          "[base][memory_mapped_file]") {
  TempFile const temp_file;
  REQUIRE(temp_file.is_valid());

  constexpr usize kFileSize = 512;

  FileHandle file;
  REQUIRE(file.open(temp_file.path(), FileAccess::ReadWrite));
  REQUIRE(file.resize(kFileSize));

  MemoryMappedFile mmap_1;
  REQUIRE(mmap_1.map(file, 0, kFileSize));
  CHECK(mmap_1.is_mapped());

  SECTION("Move constructor transfers ownership correctly") {
    MemoryMappedFile mmap_2(std::move(mmap_1));

    CHECK_FALSE(mmap_1.is_mapped());
    CHECK(mmap_1.data() == nullptr);
    CHECK(mmap_1.size() == 0);

    CHECK(mmap_2.is_mapped());
    CHECK(mmap_2.data() != nullptr);
    CHECK(mmap_2.size() == kFileSize);
  }

  SECTION("Move assignment operator unmaps previous and transfers") {
    MemoryMappedFile mmap_2;
    mmap_2 = std::move(mmap_1);

    CHECK_FALSE(mmap_1.is_mapped());
    CHECK(mmap_2.is_mapped());

    mmap_2.close();
    CHECK_FALSE(mmap_2.is_mapped());
  }
}

TEST_CASE("MemoryMappedFile offset and partial mapping",
          "[base][memory_mapped_file]") {
  TempFile const temp_file;
  REQUIRE(temp_file.is_valid());

  constexpr usize kPageSize = 4096;
  constexpr usize kTotalSize = kPageSize * 2;

  FileHandle file;
  REQUIRE(file.open(temp_file.path(), FileAccess::ReadWrite));
  REQUIRE(file.resize(kTotalSize));

  MemoryMappedFile mmap;
  // Map starting from page offset.
  REQUIRE(mmap.map(file, kPageSize, kPageSize));
  CHECK(mmap.is_mapped());
  CHECK(mmap.size() == kPageSize);

  std::span<u8> const buffer = mmap.as_span();
  buffer[0] = 'Z';

  CHECK(mmap.flush());
}

}  // namespace base
