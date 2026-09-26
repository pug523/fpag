# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# Install rules and the package config, so that a consumer can either
# add_subdirectory() this tree or find_package(fpag) against an installed copy.
#
# The two public dependencies are the interesting part. A dependency that this
# project had to build itself is added to the fpagTargets export set, together
# with its headers, so that the installed tree is self contained. A dependency
# that was found on the machine is an IMPORTED target and is instead resolved at
# find_package(fpag) time by find_dependency().

include_guard(GLOBAL)

if(NOT FPAG_INSTALL)
  return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# A dependency is either vendored, in which case it travels inside
# fpagTargets, or found on the build machine, in which case the consumer has to
# resolve it itself.
fpag_vendored_targets(FPAG_VENDORED_TARGETS)

list(FIND FPAG_VENDORED_TARGETS "fmt" fpag_fmt_is_vendored)
list(FIND FPAG_VENDORED_TARGETS "xxhash" fpag_xxhash_is_vendored)
if(fpag_fmt_is_vendored EQUAL -1)
  set(FPAG_NEEDS_FMT ON)
endif()
if(fpag_xxhash_is_vendored EQUAL -1)
  set(FPAG_NEEDS_XXHASH ON)
endif()

set(FPAG_INSTALL_TARGETS fpag ${FPAG_VENDORED_TARGETS})
list(LENGTH FPAG_VENDORED_TARGETS fpag_vendored_count)
message(STATUS "fpag install exports: fpag plus ${fpag_vendored_count} built-in dependency(ies)")

install(
  TARGETS ${FPAG_INSTALL_TARGETS}
  EXPORT fpagTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/fpag"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING
        PATTERN "*.h")

# A vendored dependency brings its own headers, otherwise find_package against
# the installed prefix would resolve the library but not its include path.
foreach(dependency IN LISTS FPAG_VENDORED_TARGETS)
  if(dependency STREQUAL "fmt")
    install(DIRECTORY "${fmt_SOURCE_DIR}/include/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            FILES_MATCHING
            PATTERN "*.h")
  elseif(dependency STREQUAL "xxhash")
    install(FILES "${FPAG_XXHASH_SOURCE_DIR}/xxh3.h"
                  "${FPAG_XXHASH_SOURCE_DIR}/xxhash.h"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  endif()
endforeach()

install(
  EXPORT fpagTargets
  FILE fpagTargets.cmake
  NAMESPACE fpag::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/fpag")

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/fpag-config.cmake.in"
  "${PROJECT_BINARY_DIR}/fpag-config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/fpag")

write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/fpag-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES "${PROJECT_BINARY_DIR}/fpag-config.cmake"
              "${PROJECT_BINARY_DIR}/fpag-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/fpag")

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
              "${PROJECT_SOURCE_DIR}/README.md"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/fpag")
