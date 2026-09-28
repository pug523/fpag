// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <vector>

#include "debug/dwarf/reader.h"
#include "fpag/base/numeric.h"
#include "fpag/io/file_handle.h"
#include "fpag/io/memory_mapped_file.h"

namespace debug::dwarf {

// A read-only view of one object file, mapped for as long as the Module lives.
//
// A Module answers two questions about a *link time* address, which is the
// address the file itself talks about: which function contains it, and which
// source position does it sit at. load_bias_to() turns a runtime address into a
// link time one, so only the caller needs to know whether the object was linked
// to be loaded anywhere.
//
// Everything a Module answers is read from the file when it is constructed. A
// lookup then touches nothing but that memory, so a caller holding one across a
// whole trace parses each object once rather than once per frame.
class Module {
 public:
  // Maps `path` and reads what it needs. loaded() reports whether there is
  // anything to ask: a file that is not an object this reader reads, or one
  // built without symbols, is not an error, it is simply no answer.
  explicit Module(const char* path);
  ~Module() = default;

  Module(const Module&) = delete;
  Module& operator=(const Module&) = delete;

  [[nodiscard]] bool loaded() const { return loaded_; }
  [[nodiscard]] const char* path() const { return path_.c_str(); }

  // True when the object was linked to be loaded at any address, so a runtime
  // address means nothing until the load base comes off it.
  [[nodiscard]] bool position_independent() const {
    return position_independent_;
  }

  // The subtraction that turns a runtime address into a link time one: the load
  // base of an object that moves, nothing of one that does not.
  [[nodiscard]] constexpr u64 load_bias_to(u64 load_base) const {
    return position_independent_ ? load_base : 0;
  }

  // The name of the function containing `link_address`, or nullptr. It comes
  // from .symtab rather than from the dynamic symbol table, which is what makes
  // it name the symbols the dynamic table leaves out: everything in a library
  // built with hidden visibility, and everything in an anonymous namespace. The
  // name is a string inside the module, mangled as the object stores it.
  [[nodiscard]] const char* function_at(u64 link_address) const;

  // The source position of `link_address`, or false when the object carries no
  // line table covering it. Most objects do; one built without debug
  // information does not.
  [[nodiscard]] bool source_at(u64 link_address, SourcePosition* out) const;

 private:
  // A section of the object: where it sits in the mapping, and how wide one
  // entry of it is for the sections that are tables.
  struct Section {
    u64 offset = 0;
    u64 size = 0;
    u64 entry_size = 0;
  };

  // One function symbol, sorted by address so that a lookup is a binary search.
  struct Function {
    u64 address = 0;
    u64 size = 0;
    const char* name = nullptr;
  };

  bool read_object();
  void read_functions();
  void read_compilation_units();
  [[nodiscard]] const u8* section_data(const Section& section) const;
  [[nodiscard]] detail::Tables detail_tables() const;

  io::FileHandle file_;
  io::MemoryMappedFile mapping_;
  std::string path_;
  bool loaded_ = false;
  bool position_independent_ = false;
  Section debug_symtab_;
  Section debug_strtab_;
  Section debug_info_;
  Section debug_abbrev_;
  Section debug_line_;
  Section debug_str_;
  Section debug_line_str_;
  Section debug_str_offsets_;
  Section debug_addr_;
  Section debug_rnglists_;
  Section debug_ranges_;
  std::vector<Function> functions_;
  std::vector<CompilationUnit> units_;
};

}  // namespace debug::dwarf
