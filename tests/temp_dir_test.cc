// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/io/temp_dir.h"

#include <cstdio>
#include <string>
#include <string_view>

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

  CHECK(dir.join("sub/a.txt") ==
        std::string(dir.path()) + "/sub/a.txt");
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

  TempDir dir(std::move(moved));
  CHECK(dir.is_valid());
  CHECK(read_file(dir.join("a.txt")) == "x");
}

}  // namespace io
