// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/io/io_util.h"

#include <string>
#include <string_view>

#include "catch2/catch_test_macros.hpp"
#include "fpag/base/numeric.h"
#include "fpag/build/build_flag.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace io {

namespace {

// A pipe is the descriptor interface a caller actually reads from, and it
// delivers in pieces, which is what the short read is about.
struct Pipe {
  Pipe() = default;

  int read_fd = -1;
  int write_fd = -1;

  bool open() {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
    int fds[2] = {-1, -1};
    if (::_pipe(fds, 4096, 0) != 0) {
      return false;
    }
    read_fd = fds[0];
    write_fd = fds[1];
#else
    int fds[2] = {-1, -1};
    if (::pipe(fds) != 0) {
      return false;
    }
    read_fd = fds[0];
    write_fd = fds[1];
#endif
    return true;
  }

  ~Pipe() {
    if (read_fd >= 0) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
      ::_close(read_fd);
#else
      ::close(read_fd);
#endif
    }
    if (write_fd >= 0) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
      ::_close(write_fd);
#else
      ::close(write_fd);
#endif
    }
  }

  // The reader only sees the end of the stream once the writer is gone, so
  // a loop that drains to zero has to close it first.
  void close_writer() {
    if (write_fd < 0) {
      return;
    }
#if FPAG_BUILD_FLAG(IS_OS_WIN)
    ::_close(write_fd);
#else
    ::close(write_fd);
#endif
    write_fd = -1;
  }

  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
};

}  // namespace

TEST_CASE("Reading a descriptor returns the bytes written", "[io]") {
  Pipe pipe;
  REQUIRE(pipe.open());
  const std::string_view payload = "fn main() {}\n";
  write(pipe.write_fd, payload.data(), payload.size());
  pipe.close_writer();

  std::string got(payload.size(), '\0');
  const isize count = read(pipe.read_fd, got.data(), got.size());
  REQUIRE(count >= 0);
  CHECK(static_cast<usize>(count) == payload.size());
  CHECK(std::string_view(got.data(), static_cast<usize>(count)) == payload);
}

// The two cases below ask a pipe for its end of stream, and the wasm runtime's
// pipe has none: read() there reports -1 past the last byte instead of zero,
// which is the same missing-POSIX kind of thing as the fork() check_test.cc
// cannot use. The read itself is covered on wasm by the case above.
#if !FPAG_BUILD_FLAG(IS_OS_ASMJS)

TEST_CASE("A read is short when the buffer is", "[io]") {
  // A caller draining a pipe has to loop, and can only do that if a short
  // read is distinguishable from the end of the stream.
  Pipe pipe;
  REQUIRE(pipe.open());
  const std::string payload(64, 'x');
  write(pipe.write_fd, payload.data(), payload.size());
  pipe.close_writer();

  char small[8] = {};
  const isize first = read(pipe.read_fd, small, sizeof(small));
  REQUIRE(first > 0);
  CHECK(static_cast<usize>(first) <= sizeof(small));

  std::string got(small, static_cast<usize>(first));
  char buffer[64];
  isize count = first;
  while (count > 0) {
    count = read(pipe.read_fd, buffer, sizeof(buffer));
    REQUIRE(count >= 0);
    got.append(buffer, static_cast<usize>(count));
  }
  CHECK(got == payload);
}

TEST_CASE("Reading a closed descriptor reports zero at end of file", "[io]") {
  Pipe pipe;
  REQUIRE(pipe.open());
  pipe.close_writer();

  char buffer[8];
  const isize count = read(pipe.read_fd, buffer, sizeof(buffer));
  CHECK(count == 0);
}

#endif  // !FPAG_BUILD_FLAG(IS_OS_ASMJS

}  // namespace io
