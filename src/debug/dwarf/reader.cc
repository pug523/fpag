// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "debug/dwarf/reader.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "fpag/base/numeric.h"

namespace debug::dwarf::detail {

namespace {

// The DWARF this reader follows. A tag, an attribute, a form and an opcode are
// all small numbers in a table of their own, so they are named here and used by
// name below. Versions 2 through 5 are read; a unit in any other version is
// skipped rather than misread.

constexpr u64 DW_TAG_COMPILE_UNIT = 0x11;

constexpr u64 DW_UT_COMPILE = 0x01;

constexpr u64 DW_AT_NAME = 0x03;
constexpr u64 DW_AT_STMT_LIST = 0x10;
constexpr u64 DW_AT_LOW_PC = 0x11;
constexpr u64 DW_AT_HIGH_PC = 0x12;
constexpr u64 DW_AT_COMP_DIR = 0x1B;
constexpr u64 DW_AT_RANGES = 0x55;
constexpr u64 DW_AT_STR_OFFSETS_BASE = 0x72;
constexpr u64 DW_AT_ADDR_BASE = 0x73;
constexpr u64 DW_AT_RNGLISTS_BASE = 0x74;

constexpr u64 DW_FORM_ADDR = 0x01;
constexpr u64 DW_FORM_BLOCK2 = 0x03;
constexpr u64 DW_FORM_BLOCK4 = 0x04;
constexpr u64 DW_FORM_DATA2 = 0x05;
constexpr u64 DW_FORM_DATA4 = 0x06;
constexpr u64 DW_FORM_DATA8 = 0x07;
constexpr u64 DW_FORM_STRING = 0x08;
constexpr u64 DW_FORM_BLOCK = 0x09;
constexpr u64 DW_FORM_BLOCK1 = 0x0A;
constexpr u64 DW_FORM_DATA1 = 0x0B;
constexpr u64 DW_FORM_FLAG = 0x0C;
constexpr u64 DW_FORM_SDATA = 0x0D;
constexpr u64 DW_FORM_STRP = 0x0E;
constexpr u64 DW_FORM_UDATA = 0x0F;
constexpr u64 DW_FORM_REF_ADDR = 0x10;
constexpr u64 DW_FORM_REF1 = 0x11;
constexpr u64 DW_FORM_REF2 = 0x12;
constexpr u64 DW_FORM_REF4 = 0x13;
constexpr u64 DW_FORM_REF8 = 0x14;
constexpr u64 DW_FORM_REF_UDATA = 0x15;
constexpr u64 DW_FORM_INDIRECT = 0x16;
constexpr u64 DW_FORM_SEC_OFFSET = 0x17;
constexpr u64 DW_FORM_EXPRLOC = 0x18;
constexpr u64 DW_FORM_FLAG_PRESENT = 0x19;
constexpr u64 DW_FORM_STRX = 0x1A;
constexpr u64 DW_FORM_ADDRX = 0x1B;
constexpr u64 DW_FORM_REF_SUP4 = 0x1C;
constexpr u64 DW_FORM_STRP_SUP = 0x1D;
constexpr u64 DW_FORM_DATA16 = 0x1E;
constexpr u64 DW_FORM_LINE_STRP = 0x1F;
constexpr u64 DW_FORM_REF_SIG8 = 0x20;
constexpr u64 DW_FORM_IMPLICIT_CONST = 0x21;
constexpr u64 DW_FORM_LOCLISTX = 0x22;
constexpr u64 DW_FORM_RNGLISTX = 0x23;
constexpr u64 DW_FORM_REF_SUP8 = 0x24;
constexpr u64 DW_FORM_STRX1 = 0x25;
constexpr u64 DW_FORM_STRX4 = 0x28;
constexpr u64 DW_FORM_ADDRX1 = 0x29;
constexpr u64 DW_FORM_ADDRX4 = 0x2C;

constexpr u64 DW_LNS_COPY = 1;
constexpr u64 DW_LNS_ADVANCE_PC = 2;
constexpr u64 DW_LNS_ADVANCE_LINE = 3;
constexpr u64 DW_LNS_SET_FILE = 4;
constexpr u64 DW_LNS_SET_COLUMN = 5;
constexpr u64 DW_LNS_NEGATE_STMT = 6;
constexpr u64 DW_LNS_SET_BASIC_BLOCK = 7;
constexpr u64 DW_LNS_CONST_ADD_PC = 8;
constexpr u64 DW_LNS_FIXED_ADVANCE_PC = 9;
constexpr u64 DW_LNS_SET_PROLOGUE_END = 10;
constexpr u64 DW_LNS_SET_EPILOGUE_BEGIN = 11;
constexpr u64 DW_LNS_SET_ISA = 12;

constexpr u64 DW_LNE_END_SEQUENCE = 1;
constexpr u64 DW_LNE_SET_ADDRESS = 2;

constexpr u64 DW_LNCT_PATH = 1;
constexpr u64 DW_LNCT_DIRECTORY_INDEX = 2;

constexpr u64 DW_RLE_END_OF_LIST = 0x00;
constexpr u64 DW_RLE_BASE_ADDRESSX = 0x01;
constexpr u64 DW_RLE_STARTX_ENDX = 0x02;
constexpr u64 DW_RLE_STARTX_LENGTH = 0x03;
constexpr u64 DW_RLE_OFFSET_PAIR = 0x04;
constexpr u64 DW_RLE_BASE_ADDRESS = 0x05;
constexpr u64 DW_RLE_START_END = 0x06;
constexpr u64 DW_RLE_START_LENGTH = 0x07;

constexpr u64 RESERVED_UNIT_LENGTH = 0xFFFFFFF0u;
constexpr u64 DWARF64_UNIT_LENGTH = 0xFFFFFFFFu;
constexpr u64 MAX_ATTRIBUTES_PER_ABBREVIATION = 24;
constexpr u64 MAX_TRACKED_ATTRIBUTES = 16;
constexpr u64 MAX_LINE_HEADER_ENTRIES = 16;
constexpr u64 MAX_STANDARD_OPCODES = 32;

// One attribute of a debug information entry, as its abbreviation declares it.
struct AbbreviationAttribute {
  u64 attribute = 0;
  u64 form = 0;
  i64 implicit = 0;
};

// One abbreviation: a tag, whether it has children, and the attributes that
// follow it in every entry using it.
struct Abbreviation {
  u64 tag = 0;
  u8 count = 0;
  AbbreviationAttribute attributes[MAX_ATTRIBUTES_PER_ABBREVIATION] = {};
};

// One value of a form. `text` is set for the string forms that resolve without
// a base, and `indexed` says the value is an index that needs one.
struct FormValue {
  u64 value = 0;
  const char* text = nullptr;
  bool indexed = false;
};

// Reads the abbreviation numbered `code` out of the table the cursor is on, and
// leaves the cursor on the entry after it. False means the table is not one
// this reader follows, and the caller must skip the entry rather than guess at
// it.
bool read_abbreviation(Cursor* cursor, u64 code, Abbreviation* out) {
  Abbreviation candidate = {};
  for (;;) {
    u64 entry_code = 0;
    if (!cursor->uleb(&entry_code) || entry_code == 0) {
      return false;
    }
    if (!cursor->uleb(&candidate.tag)) {
      return false;
    }
    u64 has_children = 0;
    if (!cursor->fixed(&has_children, 1)) {
      return false;
    }
    candidate.count = 0;
    for (;;) {
      u64 attribute = 0;
      u64 form = 0;
      if (!cursor->uleb(&attribute) || !cursor->uleb(&form)) {
        return false;
      }
      if (attribute == 0 && form == 0) {
        break;
      }
      if (candidate.count == MAX_ATTRIBUTES_PER_ABBREVIATION) {
        return false;
      }
      AbbreviationAttribute& entry = candidate.attributes[candidate.count];
      entry.attribute = attribute;
      entry.form = form;
      entry.implicit = 0;
      ++candidate.count;
      if (form == DW_FORM_IMPLICIT_CONST) {
        i64 value = 0;
        if (!cursor->sleb(&value)) {
          return false;
        }
        entry.implicit = value;
      }
    }
    if (entry_code == code) {
      *out = candidate;
      return true;
    }
  }
}

// Reads one attribute value. The forms this reader has no use for are skipped
// correctly rather than refused, so an entry carrying an unfamiliar attribute
// still parses up to the attributes that matter.
bool read_form(Cursor* cursor,
               u64 form,
               const Tables& tables,
               const AbbreviationAttribute& declaration,
               FormValue* out) {
  *out = FormValue{};
  u64 value = 0;

  if (form == DW_FORM_INDIRECT) {
    u64 indirect = 0;
    if (!cursor->uleb(&indirect)) {
      return false;
    }
    AbbreviationAttribute resolved = declaration;
    resolved.form = indirect;
    return read_form(cursor, indirect, tables, resolved, out);
  }
  if (form == DW_FORM_IMPLICIT_CONST) {
    out->value = static_cast<u64>(declaration.implicit);
    return true;
  }
  if (form == DW_FORM_FLAG_PRESENT) {
    out->value = 1;
    return true;
  }
  if (form == DW_FORM_ADDR) {
    if (!cursor->fixed(&value, tables.address_size)) {
      return false;
    }
    out->value = value;
    return true;
  }
  if (form >= DW_FORM_STRX1 && form <= DW_FORM_STRX4) {
    if (!cursor->fixed(&value, form - DW_FORM_STRX1 + 1)) {
      return false;
    }
    out->value = value;
    out->indexed = true;
    return true;
  }
  if (form >= DW_FORM_ADDRX1 && form <= DW_FORM_ADDRX4) {
    if (!cursor->fixed(&value, form - DW_FORM_ADDRX1 + 1)) {
      return false;
    }
    out->value = value;
    out->indexed = true;
    return true;
  }

  switch (form) {
    case DW_FORM_DATA1:
    case DW_FORM_FLAG:
    case DW_FORM_REF1:
      if (!cursor->fixed(&value, 1)) {
        return false;
      }
      break;
    case DW_FORM_DATA2:
    case DW_FORM_REF2:
      if (!cursor->fixed(&value, 2)) {
        return false;
      }
      break;
    case DW_FORM_DATA4:
    case DW_FORM_REF4:
    case DW_FORM_REF_SUP4:
      if (!cursor->fixed(&value, 4)) {
        return false;
      }
      break;
    case DW_FORM_DATA8:
    case DW_FORM_REF8:
    case DW_FORM_REF_SIG8:
    case DW_FORM_REF_SUP8:
      if (!cursor->fixed(&value, 8)) {
        return false;
      }
      break;
    case DW_FORM_DATA16: return cursor->skip(16);
    case DW_FORM_UDATA:
    case DW_FORM_REF_UDATA:
    case DW_FORM_LOCLISTX:
    case DW_FORM_RNGLISTX:
    case DW_FORM_STRX:
    case DW_FORM_ADDRX:
      if (!cursor->uleb(&value)) {
        return false;
      }
      out->value = value;
      out->indexed = form == DW_FORM_STRX || form == DW_FORM_ADDRX;
      return true;
    case DW_FORM_SDATA: {
      i64 signed_value = 0;
      if (!cursor->sleb(&signed_value)) {
        return false;
      }
      value = static_cast<u64>(signed_value);
      break;
    }
    case DW_FORM_SEC_OFFSET:
    case DW_FORM_STRP:
    case DW_FORM_REF_ADDR:
    case DW_FORM_LINE_STRP:
    case DW_FORM_STRP_SUP:
      if (!cursor->fixed(&value, tables.offset_size)) {
        return false;
      }
      break;
    case DW_FORM_STRING: {
      const char* text = nullptr;
      if (!cursor->string(&text)) {
        return false;
      }
      out->text = text;
      return true;
    }
    case DW_FORM_BLOCK1: {
      u64 length = 0;
      return cursor->fixed(&length, 1) && cursor->skip(length);
    }
    case DW_FORM_BLOCK2: {
      u64 length = 0;
      return cursor->fixed(&length, 2) && cursor->skip(length);
    }
    case DW_FORM_BLOCK4: {
      u64 length = 0;
      return cursor->fixed(&length, 4) && cursor->skip(length);
    }
    case DW_FORM_BLOCK:
    case DW_FORM_EXPRLOC: {
      u64 length = 0;
      return cursor->uleb(&length) && cursor->skip(length);
    }
    default: return false;
  }

  // A string named by an offset resolves now. One named by an index has to wait
  // for the base, which the entry may declare after the attribute using it.
  if (form == DW_FORM_STRP) {
    out->text = tables.debug_str.string_at(value);
  } else if (form == DW_FORM_LINE_STRP) {
    out->text = tables.debug_line_str.string_at(value);
  }
  out->value = value;
  return true;
}

// The section offset that an index into a table of offsets names. Both
// .debug_str_offsets and the offset table of .debug_rnglists are such a table:
// the index is counted from the base its unit declares, and the entry holds the
// offset itself. Zero means the index named nothing, which is also the offset
// of the header of every contribution, so nothing real is lost.
u64 indexed_offset(const SectionView& table,
                   u64 base,
                   u64 index,
                   u64 entry_size) {
  if (base == 0) {
    return 0;
  }
  Cursor offsets = table.cursor();
  u64 offset = 0;
  if (!offsets.seek(base + index * entry_size) ||
      !offsets.fixed(&offset, entry_size)) {
    return 0;
  }
  return offset;
}

// A string named by an index into .debug_str_offsets, or nullptr when the index
// names nothing.
const char* indexed_string(const Tables& tables, u64 index) {
  if (tables.str_offsets_base == 0) {
    return nullptr;
  }
  const u64 offset =
      indexed_offset(tables.debug_str_offsets, tables.str_offsets_base, index,
                     tables.offset_size);
  return offset != 0 ? tables.debug_str.string_at(offset) : nullptr;
}

// An address named by an index into .debug_addr, or false when the index names
// nothing.
bool indexed_address(const Tables& tables, u64 index, u64* out) {
  if (tables.addr_base == 0) {
    return false;
  }
  Cursor addresses = tables.debug_addr.cursor();
  if (!addresses.seek(tables.addr_base + index * tables.address_size)) {
    return false;
  }
  return addresses.fixed(out, tables.address_size);
}

// Joins a name from a line program's file table onto the directory it is
// relative to, and that directory onto the working directory of its unit when
// it is itself relative. A name that is already absolute is left alone.
std::string join_path(const Tables& tables,
                      const std::string& directory,
                      const std::string& file) {
  if (file.empty() || file.front() == '/') {
    return file;
  }
  std::string base = directory;
  if (base.empty()) {
    base = tables.working_directory;
  } else if (base.front() != '/' && !tables.working_directory.empty()) {
    base = tables.working_directory + "/" + base;
  }
  if (base.empty()) {
    return file;
  }
  if (base.back() != '/') {
    base += '/';
  }
  base += file;
  return base;
}

// The part of a line program header that a row depends on.
struct LineHeader {
  u64 version = 4;
  u64 address_size = 8;
  u64 minimum_instruction_length = 1;
  u64 maximum_operations = 1;
  i64 line_base = -5;
  u64 line_range = 14;
  u64 opcode_base = 13;
  u64 standard_lengths[MAX_STANDARD_OPCODES] = {};
  u64 program = 0;  // offset of the first opcode
  u64 end = 0;      // offset just past the unit
};

// Reads a line program header, starting at the unit's own length. When `files`
// is given it receives the unit's file table with the paths already joined,
// because a row names a file by index and only the table can say what that
// index means. When it is null the same fields are read and discarded, which is
// what a lookup of one address wants.
bool read_line_header(Cursor* cursor,
                      Tables tables,
                      std::vector<std::string>* files,
                      LineHeader* out) {
  u64 unit_length = 0;
  if (!cursor->fixed(&unit_length, 4)) {
    return false;
  }
  if (unit_length == DWARF64_UNIT_LENGTH) {
    unit_length = 0;
    if (!cursor->fixed(&unit_length, 8)) {
      return false;
    }
    tables.offset_size = 8;
  } else if (unit_length >= RESERVED_UNIT_LENGTH) {
    return false;
  }
  if (unit_length > cursor->size() - cursor->position()) {
    return false;
  }
  const u64 after_length = cursor->position();
  out->end = after_length + unit_length;

  if (!cursor->fixed(&out->version, 2) || out->version < 2 ||
      out->version > 5) {
    return false;
  }
  // From version 5 the header says how wide an address is. Before that it does
  // not, and the width is the one the unit declared.
  if (out->version >= 5) {
    u64 segment_selector_size = 0;
    if (!cursor->fixed(&out->address_size, 1) ||
        !cursor->fixed(&segment_selector_size, 1)) {
      return false;
    }
    if (out->address_size == 0 || out->address_size > 8) {
      return false;
    }
  } else {
    out->address_size = tables.address_size;
  }
  tables.address_size = out->address_size;

  u64 header_length = 0;
  if (!cursor->fixed(&header_length, tables.offset_size)) {
    return false;
  }
  if (header_length > out->end - cursor->position() ||
      out->end > cursor->size()) {
    return false;
  }
  out->program = cursor->position() + header_length;

  if (!cursor->fixed(&out->minimum_instruction_length, 1)) {
    return false;
  }
  // Operations per instruction arrived with version 4; before that a unit has
  // exactly one, which is what leaving the default says.
  if (out->version >= 4 && !cursor->fixed(&out->maximum_operations, 1)) {
    return false;
  }
  u64 default_is_stmt = 0;
  u64 line_base = 0;
  if (!cursor->fixed(&default_is_stmt, 1) || !cursor->fixed(&line_base, 1) ||
      !cursor->fixed(&out->line_range, 1) ||
      !cursor->fixed(&out->opcode_base, 1)) {
    return false;
  }
  if (out->minimum_instruction_length == 0 || out->maximum_operations == 0 ||
      out->line_range == 0 || out->opcode_base == 0 ||
      out->opcode_base > MAX_STANDARD_OPCODES) {
    return false;
  }
  for (u64 opcode = 1; opcode < out->opcode_base; ++opcode) {
    if (!cursor->fixed(&out->standard_lengths[opcode], 1)) {
      return false;
    }
  }

  // The directory table, and then the file table. Before version 5 they are a
  // list of strings and a list of index and number tuples; from 5 they are
  // entries written in forms the header itself declares.
  std::vector<std::string> directories;
  if (out->version >= 5) {
    u64 format_count = 0;
    if (!cursor->uleb(&format_count) ||
        format_count > MAX_LINE_HEADER_ENTRIES) {
      return false;
    }
    u64 content[MAX_LINE_HEADER_ENTRIES] = {};
    u64 forms[MAX_LINE_HEADER_ENTRIES] = {};
    for (u64 index = 0; index < format_count; ++index) {
      if (!cursor->uleb(&content[index]) || !cursor->uleb(&forms[index])) {
        return false;
      }
    }
    u64 count = 0;
    if (!cursor->uleb(&count)) {
      return false;
    }
    for (u64 entry = 0; entry < count; ++entry) {
      std::string path;
      for (u64 index = 0; index < format_count; ++index) {
        AbbreviationAttribute declaration = {};
        FormValue value = {};
        if (!read_form(cursor, forms[index], tables, declaration, &value)) {
          return false;
        }
        if (content[index] == DW_LNCT_PATH && value.text != nullptr) {
          path.assign(value.text);
        }
      }
      directories.push_back(std::move(path));
    }

    if (!cursor->uleb(&format_count) ||
        format_count > MAX_LINE_HEADER_ENTRIES) {
      return false;
    }
    u64 file_content[MAX_LINE_HEADER_ENTRIES] = {};
    u64 file_forms[MAX_LINE_HEADER_ENTRIES] = {};
    for (u64 index = 0; index < format_count; ++index) {
      if (!cursor->uleb(&file_content[index]) ||
          !cursor->uleb(&file_forms[index])) {
        return false;
      }
    }
    u64 file_count = 0;
    if (!cursor->uleb(&file_count)) {
      return false;
    }
    for (u64 entry = 0; entry < file_count; ++entry) {
      std::string path;
      u64 directory_index = 0;
      for (u64 index = 0; index < format_count; ++index) {
        AbbreviationAttribute declaration = {};
        FormValue value = {};
        if (!read_form(cursor, file_forms[index], tables, declaration,
                       &value)) {
          return false;
        }
        if (file_content[index] == DW_LNCT_PATH && value.text != nullptr) {
          path.assign(value.text);
        } else if (file_content[index] == DW_LNCT_DIRECTORY_INDEX) {
          directory_index = value.value;
        }
      }
      if (files != nullptr) {
        const std::string& directory =
            directory_index < directories.size()
                ? directories[static_cast<usize>(directory_index)]
                : std::string();
        files->push_back(join_path(tables, directory, path));
      }
    }
    return true;
  }

  for (;;) {
    const char* directory = nullptr;
    if (!cursor->string(&directory)) {
      return false;
    }
    if (directory[0] == '\0') {
      break;
    }
    directories.emplace_back(directory);
  }
  if (files != nullptr) {
    // Index 0 names the unit's own file, which the table does not repeat.
    files->emplace_back();
  }
  for (;;) {
    const char* path = nullptr;
    u64 directory_index = 0;
    u64 timestamp = 0;
    u64 length = 0;
    if (!cursor->string(&path)) {
      return false;
    }
    if (path[0] == '\0') {
      break;
    }
    if (!cursor->uleb(&directory_index) || !cursor->uleb(&timestamp) ||
        !cursor->uleb(&length)) {
      return false;
    }
    if (files != nullptr) {
      // Before version 5 the directory a file names is counted from one: index
      // zero is the working directory of the unit, and the table starts at one.
      const std::string& directory =
          directory_index == 0
              ? tables.working_directory
              : (directory_index - 1 < directories.size()
                     ? directories[static_cast<usize>(directory_index - 1)]
                     : std::string());
      files->push_back(join_path(tables, directory, path));
    }
  }
  return true;
}

// Walks the range list the cursor is on, calling `visit` with each address
// range it covers and stopping as soon as `visit` returns false. Entries
// written as offsets are measured from `base`, which is the unit's own low
// address.
template <typename Visit>
bool walk_ranges(Cursor* cursor, const Tables& tables, u64 base, Visit visit) {
  u64 address_base = base;
  for (;;) {
    u64 entry = 0;
    if (!cursor->uleb(&entry)) {
      return false;
    }
    if (entry == DW_RLE_END_OF_LIST) {
      return true;
    }
    u64 start = 0;
    u64 end = 0;
    u64 index = 0;
    u64 length = 0;
    switch (entry) {
      case DW_RLE_BASE_ADDRESSX:
        if (!cursor->uleb(&index) ||
            !indexed_address(tables, index, &address_base)) {
          return false;
        }
        continue;
      case DW_RLE_BASE_ADDRESS:
        if (!cursor->fixed(&address_base, tables.address_size)) {
          return false;
        }
        continue;
      case DW_RLE_OFFSET_PAIR: {
        u64 start_offset = 0;
        u64 end_offset = 0;
        if (!cursor->uleb(&start_offset) || !cursor->uleb(&end_offset)) {
          return false;
        }
        start = address_base + start_offset;
        end = address_base + end_offset;
        break;
      }
      case DW_RLE_STARTX_LENGTH:
        if (!cursor->uleb(&index) || !cursor->uleb(&length) ||
            !indexed_address(tables, index, &start)) {
          return false;
        }
        end = start + length;
        break;
      case DW_RLE_STARTX_ENDX:
        if (!cursor->uleb(&index) || !indexed_address(tables, index, &start)) {
          return false;
        }
        if (!cursor->uleb(&index) || !indexed_address(tables, index, &end)) {
          return false;
        }
        break;
      case DW_RLE_START_LENGTH:
        if (!cursor->fixed(&start, tables.address_size) ||
            !cursor->uleb(&length)) {
          return false;
        }
        end = start + length;
        break;
      case DW_RLE_START_END:
        if (!cursor->fixed(&start, tables.address_size) ||
            !cursor->fixed(&end, tables.address_size)) {
          return false;
        }
        break;
      default: return false;
    }
    if (!visit(start, end)) {
      return true;
    }
  }
}

// Walks a range list in the shape versions before 5 use: pairs of addresses,
// with two sentinels choosing the base address they are measured from.
template <typename Visit>
bool walk_legacy_ranges(Cursor* cursor,
                        u64 address_size,
                        u64 base,
                        Visit visit) {
  const u64 all_ones = address_size == 8 ? ~static_cast<u64>(0) : 0xFFFFFFFFu;
  u64 address_base = base;
  for (;;) {
    u64 start = 0;
    if (!cursor->fixed(&start, address_size)) {
      return false;
    }
    if (start == all_ones) {
      if (!cursor->fixed(&address_base, address_size)) {
        return false;
      }
      continue;
    }
    if (start == all_ones - 1) {
      return true;
    }
    u64 end = 0;
    if (!cursor->fixed(&end, address_size)) {
      return false;
    }
    if (!visit(start, end)) {
      return true;
    }
  }
}

}  // namespace

const u8* SectionView::at(u64 offset, u64 bytes) const {
  if (offset > size_ || bytes > size_ - offset) {
    return nullptr;
  }
  return data_ + offset;
}

const char* SectionView::string_at(u64 offset) const {
  if (offset >= size_) {
    return nullptr;
  }
  const void* const end =
      std::memchr(data_ + offset, '\0', static_cast<usize>(size_ - offset));
  return end != nullptr ? reinterpret_cast<const char*>(data_ + offset)
                        : nullptr;
}

Cursor SectionView::cursor() const {
  return Cursor(data_, size_);
}

bool Cursor::seek(u64 position) {
  if (position > size_) {
    return false;
  }
  position_ = position;
  return true;
}

bool Cursor::fixed(u64* out, u64 bytes) {
  if (bytes == 0 || bytes > 8 || bytes > size_ - position_) {
    *out = 0;
    return false;
  }
  u64 value = 0;
  for (u64 index = 0; index < bytes; ++index) {
    value |= static_cast<u64>(data_[position_ + index]) << (index * 8);
  }
  position_ += bytes;
  *out = value;
  return true;
}

bool Cursor::uleb(u64* out) {
  u64 value = 0;
  u64 shift = 0;
  for (u64 count = 0; count < 10; ++count) {
    u64 byte = 0;
    if (!fixed(&byte, 1)) {
      *out = 0;
      return false;
    }
    if (shift < 64) {
      value |= (byte & 0x7F) << shift;
    }
    shift += 7;
    if ((byte & 0x80) == 0) {
      *out = value;
      return true;
    }
  }
  *out = 0;
  return false;
}

bool Cursor::sleb(i64* out) {
  u64 value = 0;
  u64 shift = 0;
  u64 byte = 0;
  for (u64 count = 0; count < 10; ++count) {
    if (!fixed(&byte, 1)) {
      *out = 0;
      return false;
    }
    if (shift < 64) {
      value |= (byte & 0x7F) << shift;
    }
    shift += 7;
    if ((byte & 0x80) == 0) {
      // The last byte carries the sign in the bit above the value.
      if (shift < 64 && (byte & 0x40) != 0) {
        value |= ~static_cast<u64>(0) << shift;
      }
      *out = static_cast<i64>(value);
      return true;
    }
  }
  *out = 0;
  return false;
}

bool Cursor::skip(u64 bytes) {
  if (bytes > size_ - position_) {
    return false;
  }
  position_ += bytes;
  return true;
}

bool Cursor::string(const char** out) {
  if (position_ >= size_) {
    *out = nullptr;
    return false;
  }
  const u8* const start = data_ + position_;
  const u8* const end = static_cast<const u8*>(
      std::memchr(start, '\0', static_cast<usize>(size_ - position_)));
  if (end == nullptr) {
    *out = nullptr;
    return false;
  }
  position_ += static_cast<u64>(end - start) + 1;
  *out = reinterpret_cast<const char*>(start);
  return true;
}

bool read_unit(Cursor* info,
               const SectionView& abbrev,
               Tables tables,
               CompilationUnit* unit,
               u64* next_offset) {
  u64 unit_length = 0;
  if (!info->fixed(&unit_length, 4)) {
    return false;
  }
  if (unit_length == DWARF64_UNIT_LENGTH) {
    unit_length = 0;
    if (!info->fixed(&unit_length, 8)) {
      return false;
    }
    tables.offset_size = 8;
  } else if (unit_length >= RESERVED_UNIT_LENGTH) {
    return false;
  }
  if (unit_length > info->size() - info->position()) {
    return false;
  }
  // A unit this reader cannot parse is skipped rather than fatal: the units
  // after it are independent, and one of them may be the one asked about.
  *next_offset = info->position() + unit_length;

  u64 version = 0;
  if (!info->fixed(&version, 2) || version < 2 || version > 5) {
    return true;
  }
  if (version >= 5) {
    u64 unit_type = 0;
    if (!info->fixed(&unit_type, 1) || !info->fixed(&unit->address_size, 1)) {
      return true;
    }
    // Only a unit that is code of its own carries a line program. The others
    // are types, partial units and the halves of a split unit, all of which
    // point elsewhere for what they hold.
    if (unit_type != DW_UT_COMPILE) {
      return true;
    }
  }
  u64 abbrev_offset = 0;
  if (!info->fixed(&abbrev_offset, tables.offset_size)) {
    return true;
  }
  if (version < 5 && !info->fixed(&unit->address_size, 1)) {
    return true;
  }
  tables.address_size = unit->address_size;

  u64 code = 0;
  if (!info->uleb(&code) || code == 0) {
    return true;  // a unit with no root entry says nothing
  }
  Abbreviation declaration = {};
  {
    Cursor table = abbrev.cursor();
    if (!table.seek(abbrev_offset) ||
        !read_abbreviation(&table, code, &declaration)) {
      return true;
    }
  }
  if (declaration.tag != DW_TAG_COMPILE_UNIT) {
    return true;
  }

  // The attributes this reader wants, in the order the entry declares them. A
  // base an indexed form needs is resolved after the pass, because an entry is
  // free to declare the base after the attribute that uses it.
  struct Found {
    u64 attribute = 0;
    u64 form = 0;
    u64 value = 0;
    const char* text = nullptr;
    bool indexed = false;
  };
  Found found[MAX_TRACKED_ATTRIBUTES] = {};
  u64 found_count = 0;
  for (u64 index = 0; index < declaration.count; ++index) {
    const AbbreviationAttribute& attribute = declaration.attributes[index];
    FormValue value = {};
    if (!read_form(info, attribute.form, tables, attribute, &value)) {
      break;  // what was read before it is still worth keeping
    }
    const bool wanted = attribute.attribute == DW_AT_NAME ||
                        attribute.attribute == DW_AT_COMP_DIR ||
                        attribute.attribute == DW_AT_STMT_LIST ||
                        attribute.attribute == DW_AT_LOW_PC ||
                        attribute.attribute == DW_AT_HIGH_PC ||
                        attribute.attribute == DW_AT_RANGES ||
                        attribute.attribute == DW_AT_ADDR_BASE ||
                        attribute.attribute == DW_AT_STR_OFFSETS_BASE ||
                        attribute.attribute == DW_AT_RNGLISTS_BASE;
    if (wanted && found_count < MAX_TRACKED_ATTRIBUTES) {
      found[found_count].attribute = attribute.attribute;
      found[found_count].form = attribute.form;
      found[found_count].value = value.value;
      found[found_count].text = value.text;
      found[found_count].indexed = value.indexed;
      ++found_count;
    }
  }

  const auto lookup = [&](u64 attribute) -> const Found* {
    for (u64 index = 0; index < found_count; ++index) {
      if (found[index].attribute == attribute) {
        return &found[index];
      }
    }
    return nullptr;
  };
  const Found* const address_base = lookup(DW_AT_ADDR_BASE);
  const Found* const string_base = lookup(DW_AT_STR_OFFSETS_BASE);
  if (address_base != nullptr) {
    unit->addr_base = address_base->value;
  }
  if (string_base != nullptr) {
    unit->str_offsets_base = string_base->value;
    tables.str_offsets_base = unit->str_offsets_base;
  }
  tables.addr_base = unit->addr_base;

  const Found* const low = lookup(DW_AT_LOW_PC);
  if (low != nullptr) {
    unit->indexed_addresses = low->indexed;
    if (low->indexed) {
      if (!indexed_address(tables, low->value, &unit->low_pc)) {
        return true;
      }
    } else {
      unit->low_pc = low->value;
    }
  }
  const Found* const high = lookup(DW_AT_HIGH_PC);
  if (high != nullptr) {
    // Before version 5 the high address is written as a length from the low
    // one. From 5 it may be either, and the form is what says which.
    unit->offset_high_pc = !high->indexed && high->form != DW_FORM_ADDR;
    if (high->indexed) {
      if (!indexed_address(tables, high->value, &unit->high_pc)) {
        return true;
      }
    } else {
      unit->high_pc = high->value;
    }
  }
  const Found* const ranges = lookup(DW_AT_RANGES);
  if (ranges != nullptr) {
    // An offset of zero is the first range list, not the absence of one.
    unit->has_ranges = true;
    if (ranges->form == DW_FORM_RNGLISTX) {
      // An index into the offset table of the range lists, which the unit names
      // by the offset that table starts at. What the entry holds is measured
      // from that same base rather than from the section.
      const Found* const rnglists_base = lookup(DW_AT_RNGLISTS_BASE);
      const u64 base = rnglists_base != nullptr ? rnglists_base->value : 0;
      unit->ranges =
          indexed_offset(tables.debug_rnglists, base, ranges->value, 4) + base;
    } else {
      // Before version 5 the list is a section offset, in a section of its own
      // with an encoding of its own.
      unit->ranges = ranges->value;
      unit->legacy_ranges = version < 5;
    }
  }
  // An offset of zero is the first line program, not the absence of one, so a
  // unit that has one is flagged rather than tested by its offset.
  const Found* const statement_list = lookup(DW_AT_STMT_LIST);
  if (statement_list == nullptr ||
      statement_list->value >= tables.debug_line.size()) {
    return true;
  }
  unit->line_program = statement_list->value;
  unit->has_line_program = true;

  // A name and a working directory are only needed to turn the relative paths
  // in a file table into paths a consumer can open.
  std::string name;
  const Found* const name_entry = lookup(DW_AT_NAME);
  if (name_entry != nullptr) {
    const char* text = name_entry->text;
    if (text == nullptr && name_entry->indexed) {
      text = indexed_string(tables, name_entry->value);
    }
    if (text != nullptr) {
      name.assign(text);
    }
  }
  const Found* const directory = lookup(DW_AT_COMP_DIR);
  if (directory != nullptr) {
    const char* text = directory->text;
    if (text == nullptr && directory->indexed) {
      text = indexed_string(tables, directory->value);
    }
    if (text != nullptr) {
      tables.working_directory.assign(text);
    }
  }

  Cursor line = tables.debug_line.cursor();
  LineHeader header = {};
  if (!line.seek(unit->line_program) ||
      !read_line_header(&line, tables, &unit->files, &header)) {
    unit->files.clear();
    return true;
  }
  // Before version 5 the primary source of the unit is not in its file table:
  // index 0 stands for the name the unit itself declares.
  if (version < 5 && !name.empty() && !unit->files.empty()) {
    unit->files[0] = name;
  }
  return true;
}

bool unit_covers(const Tables& tables,
                 const CompilationUnit& unit,
                 u64 link_address) {
  if (unit.has_ranges) {
    const SectionView& list_section =
        unit.legacy_ranges ? tables.debug_ranges : tables.debug_rnglists;
    if (unit.ranges >= list_section.size()) {
      return false;
    }
    Cursor list = list_section.cursor();
    if (!list.seek(unit.ranges)) {
      return false;
    }
    bool covered = false;
    const auto visit = [&](u64 start, u64 end) {
      if (link_address >= start && link_address < end) {
        covered = true;
        return false;
      }
      return true;
    };
    if (unit.legacy_ranges) {
      walk_legacy_ranges(&list, unit.address_size, unit.low_pc, visit);
    } else {
      Tables ranged = tables;
      ranged.address_size = unit.address_size;
      ranged.addr_base = unit.addr_base;
      walk_ranges(&list, ranged, unit.low_pc, visit);
    }
    return covered;
  }
  if (unit.high_pc == 0) {
    return false;
  }
  const u64 end =
      unit.offset_high_pc ? unit.low_pc + unit.high_pc : unit.high_pc;
  return link_address >= unit.low_pc && link_address < end;
}

bool line_position(const Tables& tables,
                   const CompilationUnit& unit,
                   u64 link_address,
                   SourcePosition* out) {
  if (!unit.has_line_program || unit.files.empty() ||
      unit.line_program >= tables.debug_line.size()) {
    return false;
  }
  Cursor line = tables.debug_line.cursor();
  if (!line.seek(unit.line_program)) {
    return false;
  }
  LineHeader header = {};
  const Tables& program = tables;
  if (!read_line_header(&line, program, nullptr, &header)) {
    return false;
  }

  // The state machine, and the row it currently has open. A row covers the
  // addresses from its own address to the address of the next one, so a lookup
  // is a walk that answers as soon as a row closes over the address asked for.
  // The operation index of the machine is not kept: it says which row of a
  // vector instruction a row belongs to, and nothing reported here reads it.
  u64 address = 0;
  u64 file = 1;
  i64 line_number = 1;
  u64 column = 0;
  bool row_open = false;
  u64 row_address = 0;
  u64 row_file = 1;
  i64 row_line = 1;
  u64 row_column = 0;
  bool answered = false;

  const auto open_row = [&]() {
    row_address = address;
    row_file = file;
    row_line = line_number;
    row_column = column;
    row_open = true;
  };
  // Answers the query when the open row is the one that ends at `end`.
  const auto close_row_at = [&](u64 end) {
    if (!row_open || link_address < row_address || link_address >= end ||
        row_file >= unit.files.size() || row_line <= 0) {
      return;
    }
    const std::string& file = unit.files[static_cast<usize>(row_file)];
    if (file.empty()) {
      return;
    }
    out->file = file.c_str();
    out->line = static_cast<u32>(row_line);
    out->column = static_cast<u32>(row_column);
    answered = true;
  };
  // A row covers the addresses from its own address up to the address of the
  // next row, which is why an advance that emits no row closes nothing: the
  // open row simply reaches further.
  const auto advance = [&](u64 operation_advance) {
    address += (operation_advance / header.maximum_operations) *
               header.minimum_instruction_length;
  };
  // Answers the query when the open row is the one that ends at the current
  // address, and starts the row that takes over from it.
  const auto emit_row = [&]() {
    close_row_at(address);
    open_row();
  };

  u64 position = header.program;
  while (position < header.end && !answered) {
    if (!line.seek(position)) {
      return false;
    }
    // An opcode is one byte. A special opcode above 127 is ordinary, which is
    // why it is not read as a variable length number.
    u64 opcode = 0;
    if (!line.fixed(&opcode, 1)) {
      return false;
    }

    if (opcode >= header.opcode_base) {
      const u64 adjusted = opcode - header.opcode_base;
      advance(adjusted / header.line_range);
      line_number +=
          header.line_base + static_cast<i64>(adjusted % header.line_range);
      emit_row();
    } else if (opcode == 0) {
      // An extended opcode declares the length of everything after that, so
      // where the next one starts is known before any of it is read.
      u64 length = 0;
      if (!line.uleb(&length) || length > header.end - line.position()) {
        return false;
      }
      const u64 end = line.position() + length;
      u64 sub_opcode = 0;
      if (!line.fixed(&sub_opcode, 1)) {
        return false;
      }
      if (sub_opcode == DW_LNE_END_SEQUENCE) {
        // The row that ends a sequence is a row, and it covers the addresses
        // between the last row before it and the first row after it.
        emit_row();
        address = 0;
        file = 1;
        line_number = 1;
        column = 0;
      } else if (sub_opcode == DW_LNE_SET_ADDRESS) {
        // The operand is the address itself, at the width the header gave.
        if (!line.fixed(&address, header.address_size)) {
          return false;
        }
        emit_row();
      }
      // Any other extended opcode, including the file a unit adds part way
      // through its program, is described by its length: the cursor steps over
      // its operands.
      if (!line.seek(end)) {
        return false;
      }
    } else {
      u64 operand = 0;
      switch (opcode) {
        case DW_LNS_COPY: emit_row(); break;
        case DW_LNS_ADVANCE_PC:
          if (!line.uleb(&operand)) {
            return false;
          }
          advance(operand);
          break;
        case DW_LNS_ADVANCE_LINE: {
          i64 delta = 0;
          if (!line.sleb(&delta)) {
            return false;
          }
          line_number += delta;
          break;
        }
        case DW_LNS_SET_FILE:
          if (!line.uleb(&operand)) {
            return false;
          }
          file = operand;
          break;
        case DW_LNS_SET_COLUMN:
          if (!line.uleb(&operand)) {
            return false;
          }
          column = operand;
          break;
        case DW_LNS_CONST_ADD_PC: {
          const u64 adjusted = 255 - header.opcode_base;
          advance(adjusted / header.line_range);
          break;
        }
        case DW_LNS_FIXED_ADVANCE_PC:
          if (!line.fixed(&operand, 2)) {
            return false;
          }
          address += operand;
          break;
        case DW_LNS_SET_ISA:
        case DW_LNS_NEGATE_STMT:
        case DW_LNS_SET_BASIC_BLOCK:
        case DW_LNS_SET_PROLOGUE_END:
        case DW_LNS_SET_EPILOGUE_BEGIN:
          // None of these changes the address, the line, the file or the
          // column, which is all this reader reports. The one that carries an
          // operand is the exception.
          if (opcode == DW_LNS_SET_ISA && !line.uleb(&operand)) {
            return false;
          }
          break;
        default: {
          // A standard opcode from a newer producer. It declares how many
          // operands it takes, and they are read and dropped.
          for (u64 count = 0; count < header.standard_lengths[opcode];
               ++count) {
            if (!line.uleb(&operand)) {
              return false;
            }
          }
          break;
        }
      }
    }
    // The next opcode follows the operands, which for a special opcode is the
    // opcode itself.
    position = line.position();
  }

  return answered;
}

}  // namespace debug::dwarf::detail
