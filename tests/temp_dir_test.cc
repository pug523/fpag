// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/io/temp_dir.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"

namespace io {

namespace {

std::string read_file(std::string_view path) {
  std::FILE* file = std::fopen(std::string(path).c_str(), "rb");
  if (file == nullptr) {
    return {};
  }
  std::string out;
  char buf[256];
  usize got = 0;
  while ((got = std::fread(buf, 1, sizeof(buf), file)) > 0) {
    out.append(buf, got);
  }
  std::fclose(file);
  return out;
}

}  // namespace

TEST_CASE("TempDir creates and cleans scratch trees", "[io][temp_dir]") {
  TempDir dir("fpag_temp_dir_test");
  REQUIRE(dir.is_valid());

  CHECK(dir.make_dir("sub"));
  CHECK(dir.write_file("sub/a.txt", "hello"));
  CHECK(dir.write_file("top.txt", ""));

  CHECK(dir.join("sub/a.txt") == std::string(dir.path()) + "/sub/a.txt");
  CHECK(read_file(dir.join("sub/a.txt")) == "hello");
  CHECK(read_file(dir.join("top.txt")).empty());

  const std::string doomed = std::string(dir.path());
  dir.remove();
  CHECK(!dir.is_valid());
  CHECK(dir.path().empty());

  // Removal is observable: the root no longer opens.
  std::FILE* probe = std::fopen(doomed.c_str(), "rb");
  CHECK(probe == nullptr);
}

TEST_CASE("TempDir supports moves", "[io][temp_dir]") {
  TempDir moved("fpag_temp_dir_move_test");
  REQUIRE(moved.is_valid());
  REQUIRE(moved.write_file("a.txt", "x"));

  const TempDir dir(std::move(moved));
  CHECK(dir.is_valid());
  CHECK(read_file(dir.join("a.txt")) == "x");
}

TEST_CASE("TempDir removes a tree it did not record", "[io][temp_dir]") {
  // `write_file` creates the directories a nested path needs without
  // recording them, and a caller may write into the directory by other
  // means. Removal has to reach both, or the root is left non-empty and
  // the whole tree is leaked once per use.
  TempDir dir("fpag_temp_dir_nested_test");
  REQUIRE(dir.is_valid());
  REQUIRE(dir.write_file("a/b/c/deep.txt", "deep"));

  const std::string stranger = dir.join("stranger.o");
  std::FILE* file = std::fopen(stranger.c_str(), "wb");
  REQUIRE(file != nullptr);
  CHECK(std::fwrite("o", 1, 1, file) == 1);
  CHECK(std::fclose(file) == 0);

  const std::string doomed = std::string(dir.path());
  const std::string doomed_deep = dir.join("a/b/c/deep.txt");
  dir.remove();

  CHECK(!dir.is_valid());
  CHECK(dir.path().empty());
  // Removal is observable: neither the root nor anything under it opens.
  CHECK(std::fopen(doomed.c_str(), "rb") == nullptr);
  CHECK(std::fopen(doomed_deep.c_str(), "rb") == nullptr);
  CHECK(std::fopen(stranger.c_str(), "rb") == nullptr);
}

TEST_CASE("TempDir removal is idempotent", "[io][temp_dir]") {
  TempDir dir("fpag_temp_dir_twice_test");
  REQUIRE(dir.is_valid());
  dir.remove();
  // A second removal, and the destructor that follows it, must be no-ops
  // rather than errors on a path that is already gone.
  dir.remove();
  CHECK(!dir.is_valid());
}

}  // namespace io
