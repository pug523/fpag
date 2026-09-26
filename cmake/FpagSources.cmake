# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# The file lists the developer tooling acts on.
#
# One place, so that format, tidy and lint cannot drift apart, and so that the
# developer target definitions do not each carry their own copy of it.
#
# There are two lists, because the tools do not want the same thing.
# clang-format and cpplint read a file on its own and are happy with headers.
# clang-tidy needs a compilation database entry, which only a translation unit
# has, so it is given the .cc files and reaches the headers through them. Handing
# it a header directly also bypasses HeaderFilterRegex, because the main file is
# always checked, which would pull in the whole public header tree.

include_guard(GLOBAL)

# fpag_tooling_sources(<source_dir> <out_var>)
#
# Every public header and implementation file. third_party/ is left out because
# it carries a .clang-format-ignore precisely so a vendored tree is skipped, and
# the module directories are named explicitly rather than globbed at the
# repository root so that a stray file outside them is not silently picked up.
function(fpag_tooling_sources source_dir out_var)
  set(sources "")

  file(GLOB_RECURSE public_headers "${source_dir}/include/*.h")
  list(APPEND sources ${public_headers})

  foreach(root IN ITEMS src tests benchmarks)
    file(GLOB_RECURSE found "${source_dir}/${root}/*.cc"
                             "${source_dir}/${root}/*.h")
    list(APPEND sources ${found})
  endforeach()

  # A stable order keeps the output of a failing check reproducible.
  list(SORT sources)
  set(${out_var}
      "${sources}"
      PARENT_SCOPE)
endfunction()

# fpag_translation_units(<source_dir> <out_var>)
#
# The subset of the above that has a translation unit, which is what clang-tidy
# has to be given.
function(fpag_translation_units source_dir out_var)
  set(sources "")

  foreach(root IN ITEMS src tests benchmarks)
    file(GLOB_RECURSE found "${source_dir}/${root}/*.cc")
    list(APPEND sources ${found})
  endforeach()

  list(SORT sources)
  set(${out_var}
      "${sources}"
      PARENT_SCOPE)
endfunction()
