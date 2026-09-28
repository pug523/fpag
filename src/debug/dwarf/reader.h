// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <vector>

#include "fpag/base/numeric.h"

namespace debug::dwarf {

// A source position inside a module. `file` points into the module, so it stays
// valid for as long as the Module that produced it.
struct SourcePosition {
  const char* file = nullptr;
  u32 line = 0;
  u32 column = 0;
};

// One compilation unit: where its line program is, and which addresses it
// covers. A unit that says so with a range list carries that list's offset
// instead, which is what a compiler emitting function sections produces.
//
// The file table is materialised when the unit is read, because a row names a
// file by index and the path that index means is a string the module has to
// own.
struct CompilationUnit {
  u64 line_program = 0;
  u64 ranges = 0;
  u64 low_pc = 0;
  u64 high_pc = 0;
  u64 addr_base = 0;
  u64 str_offsets_base = 0;
  u64 address_size = 8;
  bool offset_high_pc = false;
  bool indexed_addresses = false;
  bool has_line_program = false;
  bool has_ranges = false;
  bool legacy_ranges = false;
  std::vector<std::string> files;
};

namespace detail {

class Cursor;

// A bounded view of one section of an object file. Every read checks the
// bounds, so a file that is not what it claims to be ends the parse rather than
// reading past the mapping.
class SectionView {
 public:
  SectionView() = default;
  SectionView(const u8* data, u64 size) : data_(data), size_(size) {}

  [[nodiscard]] bool present() const { return data_ != nullptr; }
  [[nodiscard]] const u8* data() const { return data_; }
  [[nodiscard]] u64 size() const { return size_; }

  // The bytes at `offset`, or nullptr when they are not all inside the section.
  [[nodiscard]] const u8* at(u64 offset, u64 bytes) const;

  // A NUL terminated string inside the section, or nullptr when it is not
  // terminated before the end. The result points into the mapping, which is
  // what the callers store.
  [[nodiscard]] const char* string_at(u64 offset) const;

  // A cursor over the whole section, positioned at its first byte.
  [[nodiscard]] Cursor cursor() const;

 private:
  const u8* data_ = nullptr;
  u64 size_ = 0;
};

// A forward cursor over a section. Every read reports whether it fit, and a
// caller that ignores the answer gets zeros rather than bytes from elsewhere.
class Cursor {
 public:
  Cursor() = default;
  Cursor(const u8* data, u64 size) : data_(data), size_(size) {}

  [[nodiscard]] u64 position() const { return position_; }
  [[nodiscard]] u64 size() const { return size_; }

  bool seek(u64 position);

  // A little endian unsigned integer of 1, 2, 4 or 8 bytes, which is how every
  // fixed width field of both formats is read.
  bool fixed(u64* out, u64 bytes);
  bool uleb(u64* out);
  bool sleb(i64* out);
  bool skip(u64 bytes);
  bool string(const char** out);

 private:
  const u8* data_ = nullptr;
  u64 size_ = 0;
  u64 position_ = 0;
};

// The sections a unit's values resolve against, and the bases it declares for
// its indexed forms. One of these is built per lookup rather than stored: it is
// a handful of views, and the alternative is a reader type inside a class a
// header declares.
struct Tables {
  SectionView debug_str;
  SectionView debug_line_str;
  SectionView debug_str_offsets;
  SectionView debug_addr;
  SectionView debug_line;
  SectionView debug_rnglists;
  SectionView debug_ranges;
  std::string working_directory;
  u64 str_offsets_base = 0;
  u64 addr_base = 0;
  u64 address_size = 8;
  u64 offset_size = 4;
};

// Reads the compilation unit the cursor is positioned at the start of, and
// leaves `next_offset` on the unit that follows it. False means the unit could
// not be read, and the caller has run out of what this reader can answer.
bool read_unit(Cursor* info,
               const SectionView& abbrev,
               Tables tables,
               CompilationUnit* unit,
               u64* next_offset);

// True when the unit says it covers `link_address`.
bool unit_covers(const Tables& tables,
                 const CompilationUnit& unit,
                 u64 link_address);

// The source position of `link_address` inside `unit`, or false when the unit's
// line program says nothing about it.
bool line_position(const Tables& tables,
                   const CompilationUnit& unit,
                   u64 link_address,
                   SourcePosition* out);

}  // namespace detail
}  // namespace debug::dwarf
