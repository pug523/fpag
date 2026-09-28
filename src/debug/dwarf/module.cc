// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "debug/dwarf/module.h"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <utility>

#include "debug/dwarf/reader.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/io/file_handle.h"

namespace debug::dwarf {

#if FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)

namespace {

// The pieces of ELF64 this file reads. Only the 64 bit little endian form is
// read: a 32 bit object is a different layout, and no platform whose addresses
// these answers are about has a big endian one. An object in any other form is
// reported as unloaded rather than misread.

constexpr u64 ELF_MAGIC = 0x464C457F;  // 0x7F 'E' 'L' 'F'
constexpr u8 ELF_CLASS_64 = 2;
constexpr u8 ELF_DATA_LSB = 1;
constexpr u16 ELF_TYPE_DYN = 3;

constexpr u32 SHT_SYMTAB = 2;
constexpr u32 SHT_STRTAB = 3;

constexpr u8 STT_FUNC = 2;
constexpr u8 STB_GLOBAL = 1;
constexpr u8 STB_WEAK = 2;

constexpr u64 ELF_SECTION_ENTRY_SIZE = 64;
constexpr u64 ELF_SYMBOL_SIZE = 24;

struct SectionHeader {
  u64 name = 0;
  u64 type = 0;
  u64 offset = 0;
  u64 size = 0;
  u64 link = 0;
  u64 entry_size = 0;
};

// Reads one section header. Every field is little endian, in the order the
// format declares them, so the header is read rather than reinterpreted: a
// struct copied out of the mapping would rest on the compiler's padding.
bool read_section_header(const u8* data,
                         u64 size,
                         u64 offset,
                         SectionHeader* out) {
  detail::Cursor cursor(data, size);
  return cursor.seek(offset) && cursor.fixed(&out->name, 4) &&
         cursor.fixed(&out->type, 4) && cursor.skip(8) && cursor.skip(8) &&
         cursor.fixed(&out->offset, 8) && cursor.fixed(&out->size, 8) &&
         cursor.fixed(&out->link, 4) && cursor.skip(4) && cursor.skip(8) &&
         cursor.fixed(&out->entry_size, 8);
}

// The bytes of one section of the mapping, or an empty view when the section
// does not fit inside it.
detail::SectionView view_of(const u8* data, u64 size, const SectionHeader& s) {
  if (s.size == 0 || s.offset > size || s.size > size - s.offset) {
    return {};
  }
  return {data + s.offset, s.size};
}

}  // namespace

Module::Module(const char* path) : path_(path != nullptr ? path : "") {
  if (path_.empty() || !file_.open(path_, io::FileAccess::Read)) {
    return;
  }
  const usize size = file_.get_size();
  if (size == 0 || !mapping_.map(file_, 0, size)) {
    file_.close();
    return;
  }
  loaded_ = read_object();
}

const u8* Module::section_data(const Section& section) const {
  if (section.size == 0 || section.offset > mapping_.size() ||
      section.size > mapping_.size() - section.offset) {
    return nullptr;
  }
  return mapping_.data() + section.offset;
}

bool Module::read_object() {
  const u64 size = mapping_.size();
  detail::Cursor cursor(mapping_.data(), size);
  u64 magic = 0;
  u64 class_byte = 0;
  u64 data_byte = 0;
  u64 type = 0;
  u64 section_table = 0;
  u64 section_entry_size = 0;
  u64 section_count = 0;
  u64 section_names = 0;
  // The header fields this reader has no use for are read into one scratch
  // value, so the cursor lands where the next field starts either way.
  u64 unused = 0;
  const bool header_read =
      cursor.fixed(&magic, 4) && cursor.fixed(&class_byte, 1) &&
      cursor.fixed(&data_byte, 1) && cursor.skip(10) &&
      cursor.fixed(&type, 2) && cursor.fixed(&unused, 2) &&
      cursor.fixed(&unused, 4) && cursor.fixed(&unused, 8) &&
      cursor.fixed(&unused, 8) && cursor.fixed(&section_table, 8) &&
      cursor.fixed(&unused, 4) && cursor.fixed(&unused, 2) &&
      cursor.fixed(&unused, 2) && cursor.fixed(&unused, 2) &&
      cursor.fixed(&section_entry_size, 2) && cursor.fixed(&section_count, 2) &&
      cursor.fixed(&section_names, 2);
  if (!header_read || magic != ELF_MAGIC || class_byte != ELF_CLASS_64 ||
      data_byte != ELF_DATA_LSB) {
    return false;
  }
  position_independent_ = type == ELF_TYPE_DYN;
  if (section_entry_size != ELF_SECTION_ENTRY_SIZE || section_count == 0 ||
      section_count > size / ELF_SECTION_ENTRY_SIZE || section_table > size) {
    return false;
  }

  const auto section_at = [&](u64 index, SectionHeader* out) {
    return read_section_header(mapping_.data(), size,
                               section_table + index * section_entry_size, out);
  };

  // The name of a section lives in a string table the header points at by
  // index, so the table is read once and the sections are matched against it.
  SectionHeader names = {};
  if (!section_at(section_names, &names) || names.type != SHT_STRTAB) {
    return false;
  }
  const detail::SectionView name_table = view_of(mapping_.data(), size, names);

  struct Wanted {
    const char* name;
    Section* out;
  };
  const Wanted wanted[] = {
      {".symtab", &debug_symtab_},
      {".debug_info", &debug_info_},
      {".debug_abbrev", &debug_abbrev_},
      {".debug_line", &debug_line_},
      {".debug_str", &debug_str_},
      {".debug_line_str", &debug_line_str_},
      {".debug_str_offsets", &debug_str_offsets_},
      {".debug_addr", &debug_addr_},
      {".debug_rnglists", &debug_rnglists_},
      {".debug_ranges", &debug_ranges_},
  };
  for (u64 index = 0; index < section_count; ++index) {
    SectionHeader section = {};
    if (!section_at(index, &section)) {
      return false;
    }
    const char* const name = name_table.string_at(section.name);
    if (name == nullptr) {
      continue;
    }
    for (const Wanted& entry : wanted) {
      if (std::string_view(name) != entry.name) {
        continue;
      }
      entry.out->offset = section.offset;
      entry.out->size = section.size;
      entry.out->entry_size = section.entry_size;
      // The strings a symbol table names live in the table its own header
      // points at, which is the only way to find them: the section need not be
      // called .strtab, and an object can have more than one.
      if (section.type == SHT_SYMTAB) {
        SectionHeader strings = {};
        if (section_at(section.link, &strings) && strings.type == SHT_STRTAB) {
          debug_strtab_.offset = strings.offset;
          debug_strtab_.size = strings.size;
        }
      }
      break;
    }
  }

  read_functions();
  read_compilation_units();
  // A module that answers nothing is the same as one that was never there, and
  // the caller has a fallback for that.
  return !functions_.empty() || !units_.empty();
}

void Module::read_functions() {
  const detail::SectionView symbols(section_data(debug_symtab_),
                                    debug_symtab_.size);
  const detail::SectionView strings(section_data(debug_strtab_),
                                    debug_strtab_.size);
  if (!symbols.present() || !strings.present()) {
    return;
  }
  const u64 entry_size = debug_symtab_.entry_size != 0
                             ? debug_symtab_.entry_size
                             : ELF_SYMBOL_SIZE;
  if (entry_size < ELF_SYMBOL_SIZE) {
    return;
  }
  const u64 count = debug_symtab_.size / entry_size;
  for (u64 index = 0; index < count; ++index) {
    detail::Cursor symbol = symbols.cursor();
    u64 name_offset = 0;
    u64 info = 0;
    u64 value = 0;
    u64 length = 0;
    if (!symbol.seek(index * entry_size) || !symbol.fixed(&name_offset, 4) ||
        !symbol.fixed(&info, 1) || !symbol.skip(3) ||
        !symbol.fixed(&value, 8) || !symbol.fixed(&length, 8)) {
      return;
    }
    if ((info & 0xF) != STT_FUNC || value == 0 || length == 0) {
      continue;
    }
    // A local symbol is what an anonymous namespace and a static function
    // produce, and those are exactly the names the dynamic symbol table is
    // missing, so they are the point of reading this table at all.
    const u64 binding = info >> 4;
    if (binding != 0 && binding != STB_GLOBAL && binding != STB_WEAK) {
      continue;
    }
    const char* const name = strings.string_at(name_offset);
    if (name == nullptr || name[0] == '\0') {
      continue;
    }
    functions_.push_back(Function{value, length, name});
  }
  // The table is not required to be in address order, so it is put in order
  // once here and every later lookup is a binary search.
  std::sort(functions_.begin(), functions_.end(),
            [](const Function& left, const Function& right) {
              return left.address < right.address;
            });
}

const char* Module::function_at(u64 link_address) const {
  if (functions_.empty()) {
    return nullptr;
  }
  // The last symbol starting at or before the address is the only one that can
  // contain it, and its size is what says whether it does.
  const auto found =
      std::upper_bound(functions_.begin(), functions_.end(), link_address,
                       [](u64 address, const Function& entry) {
                         return address < entry.address;
                       });
  if (found == functions_.begin()) {
    return nullptr;
  }
  const Function& candidate = *std::prev(found);
  if (link_address >= candidate.address + candidate.size) {
    return nullptr;
  }
  return candidate.name;
}

void Module::read_compilation_units() {
  const detail::SectionView info(section_data(debug_info_), debug_info_.size);
  const detail::SectionView abbrev(section_data(debug_abbrev_),
                                   debug_abbrev_.size);
  if (!info.present() || !abbrev.present()) {
    return;
  }
  const detail::Tables tables = detail_tables();
  detail::Cursor cursor = info.cursor();
  u64 offset = 0;
  while (offset < info.size()) {
    CompilationUnit unit;
    u64 next_offset = 0;
    if (!cursor.seek(offset) ||
        !detail::read_unit(&cursor, abbrev, tables, &unit, &next_offset) ||
        next_offset <= offset) {
      return;
    }
    if (unit.has_line_program) {
      units_.push_back(std::move(unit));
    }
    offset = next_offset;
  }
}

detail::Tables Module::detail_tables() const {
  detail::Tables tables;
  tables.debug_str = {section_data(debug_str_), debug_str_.size};
  tables.debug_line_str = {section_data(debug_line_str_), debug_line_str_.size};
  tables.debug_str_offsets = {section_data(debug_str_offsets_),
                              debug_str_offsets_.size};
  tables.debug_addr = {section_data(debug_addr_), debug_addr_.size};
  tables.debug_line = {section_data(debug_line_), debug_line_.size};
  tables.debug_rnglists = {section_data(debug_rnglists_), debug_rnglists_.size};
  tables.debug_ranges = {section_data(debug_ranges_), debug_ranges_.size};
  return tables;
}

bool Module::source_at(u64 link_address, SourcePosition* out) const {
  if (units_.empty()) {
    return false;
  }
  const detail::Tables tables = detail_tables();
  for (const CompilationUnit& unit : units_) {
    if (!detail::unit_covers(tables, unit, link_address)) {
      continue;
    }
    if (detail::line_position(tables, unit, link_address, out)) {
      return true;
    }
  }
  return false;
}

#endif  // FPAG_BUILD_FLAG(IS_OS_LINUX) || FPAG_BUILD_FLAG(IS_OS_ANDROID)

}  // namespace debug::dwarf
