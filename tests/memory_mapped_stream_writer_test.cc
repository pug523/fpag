// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/io/memory_mapped_stream_writer.h"

#include <cstring>
#include <string>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/io/file_handle.h"
#include "fpag/io/memory_mapped_file.h"
#include "fpag/io/temp_file.h"

namespace io {

namespace {

constexpr std::string_view SMALL_TEXT = "Hello, MemoryMappedStreamWriter!";

}  // namespace

TEST_CASE("MemoryMappedStreamWriter basic open and write",
          "[base][mmap_stream_writer]") {
  const TempFile temp_file;
  REQUIRE(temp_file.is_valid());

  MemoryMappedStreamWriter writer;

  SECTION("Open invalid path fails") {
    CHECK_FALSE(writer.open("/non_existent_dir_12345/test.bin"));
  }

  SECTION("Write small content and verify file size on finish") {
    REQUIRE(writer.open(temp_file.path(), /*initial_capacity=*/1024));

    REQUIRE(writer.write(SMALL_TEXT.data(), SMALL_TEXT.size()));
    CHECK(writer.bytes_written() == SMALL_TEXT.size());

    writer.finish();

    // Verify file on disk is truncated to exact written size
    FileHandle read_handle;
    REQUIRE(read_handle.open(temp_file.path(), FileAccess::Read));
    CHECK(read_handle.get_size() == SMALL_TEXT.size());

    MemoryMappedFile reader;
    REQUIRE(reader.map(read_handle, 0, 0));
    std::string_view file_content(reinterpret_cast<const char*>(reader.data()),
                                  reader.size());
    CHECK(file_content == SMALL_TEXT);
  }
}

TEST_CASE("MemoryMappedStreamWriter automatic capacity expansion",
          "[base][mmap_stream_writer]") {
  const TempFile temp_file;
  REQUIRE(temp_file.is_valid());

  MemoryMappedStreamWriter writer;
  constexpr usize INITIAL_CAPACITY =
      64;  // Small initial capacity to trigger resize easily
  REQUIRE(writer.open(temp_file.path(), INITIAL_CAPACITY));

  // Generate data exceeding initial capacity
  std::string large_data(2048, 'A');

  SECTION("Write data requiring multiple dynamic resizes") {
    REQUIRE(writer.write(large_data.data(), large_data.size()));
    CHECK(writer.bytes_written() == large_data.size());

    writer.finish();

    FileHandle read_handle;
    REQUIRE(read_handle.open(temp_file.path(), FileAccess::Read));
    CHECK(read_handle.get_size() == large_data.size());

    MemoryMappedFile reader;
    REQUIRE(reader.map(read_handle, 0, 0));
    std::string_view file_content(reinterpret_cast<const char*>(reader.data()),
                                  reader.size());
    CHECK(file_content == large_data);
  }
}

TEST_CASE("MemoryMappedStreamWriter zero-copy direct buffer formatting",
          "[base][mmap_stream_writer]") {
  const TempFile temp_file;
  REQUIRE(temp_file.is_valid());

  MemoryMappedStreamWriter writer;
  REQUIRE(writer.open(temp_file.path(), /*initial_capacity=*/128));

  constexpr usize REQUESTED_LENGTH = 32;
  u8* dest = writer.prepare_write_buffer(REQUESTED_LENGTH);
  REQUIRE(dest != nullptr);

  constexpr std::string_view DIRECT_DATA = "Direct zero-copy output";
  std::memcpy(dest, DIRECT_DATA.data(), DIRECT_DATA.size());
  writer.commit_write(DIRECT_DATA.size());

  CHECK(writer.bytes_written() == DIRECT_DATA.size());

  writer.finish();

  FileHandle read_handle;
  REQUIRE(read_handle.open(temp_file.path(), FileAccess::Read));
  MemoryMappedFile reader;
  REQUIRE(reader.map(read_handle, 0, 0));

  std::string_view file_content(reinterpret_cast<const char*>(reader.data()),
                                reader.size());
  CHECK(file_content == DIRECT_DATA);
}

TEST_CASE("MemoryMappedStreamWriter refuses to write before open",
          "[base][mmap_stream_writer]") {
  MemoryMappedStreamWriter writer;

  // The capacity is zero until open() maps a file, and growing it still has to
  // make progress instead of doubling zero.
  CHECK_FALSE(writer.write(SMALL_TEXT.data(), SMALL_TEXT.size()));
  CHECK(writer.prepare_write_buffer(SMALL_TEXT.size()) == nullptr);
  CHECK(writer.bytes_written() == 0);
}

}  // namespace io
