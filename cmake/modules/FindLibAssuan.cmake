# SPDX-FileCopyrightText: 2022 g10 Code GmbH
# SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>
#
# SPDX-License-Identifier: BSD-3-Clause

#[=======================================================================[.rst:
FindLibAssuan
-------------

Finds the Libassuan library.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``LibAssuan_FOUND``
    True if the system has the Libassuan library.
``LibAssuan_VERSION``
    The version of the Libassuan library which was found.
``LibAssuan_INCLUDE_DIRS``
    Include directories needed to use Libassuan.
``LibAssuan_LIBRARIES``
    Libraries needed to link to Libassuan.
``LibAssuan_DEFINITIONS``
    The compile definitions to use when compiling code that uses Libassuan.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``LibAssuan_INCLUDE_DIR``
    The directory containing ``assuan.h``.
``LibAssuan_LIBRARY``
    The path to the Libassuan library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LibAssuan QUIET libassuan)

set(LibAssuan_VERSION ${PC_LibAssuan_VERSION})
set(LibAssuan_DEFINITIONS ${PC_LibAssuan_CFLAGS_OTHER})

find_path(LibAssuan_INCLUDE_DIR
    NAMES
        assuan.h
    PATHS
        ${PC_LibAssuan_INCLUDE_DIRS}
)
find_library(LibAssuan_LIBRARY
    NAMES
        assuan
    PATHS
        ${PC_LibAssuan_LIBRARY_DIRS}
)

if(WIN32)
    set(_LibAssuan_ADDITIONAL_LIBRARIES ws2_32)
endif()

if(LibAssuan_INCLUDE_DIR AND NOT LibAssuan_VERSION)
    # The version is given in the format MAJOR.MINOR.PATCH optionally followed
    # by an intermediate "beta" version given as -betaNUM, e.g. "2.5.6-beta38".
    file(STRINGS "${LibAssuan_INCLUDE_DIR}/assuan.h" LibAssuan_VERSION_STR
         REGEX "^#[\t ]*define[\t ]+ASSUAN_VERSION[\t ]+\"([0-9])+\\.([0-9])+\\.([0-9])+(-[a-z0-9]*)?\".*")
    string(REGEX REPLACE "^.*ASSUAN_VERSION[\t ]+\"([0-9]+\\.[0-9]+\\.[0-9]+(-[a-z0-9]*)?)\".*$"
           "\\1" LibAssuan_VERSION_STR "${LibAssuan_VERSION_STR}")

    set(LibAssuan_VERSION "${LibAssuan_VERSION_STR}")

    unset(LibAssuan_VERSION_STR)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibAssuan
    REQUIRED_VARS
        LibAssuan_LIBRARY
        LibAssuan_INCLUDE_DIR
        LibAssuan_VERSION
    VERSION_VAR
        LibAssuan_VERSION
)

mark_as_advanced(
    LibAssuan_INCLUDE_DIR
    LibAssuan_LIBRARY
)

if(LibAssuan_FOUND)
    set(LibAssuan_LIBRARIES ${LibAssuan_LIBRARY} ${_LibAssuan_ADDITIONAL_LIBRARIES})
    set(LibAssuan_INCLUDE_DIRS ${LibAssuan_INCLUDE_DIR})
endif()

include(FeatureSummary)
set_package_properties(LibAssuan PROPERTIES
    DESCRIPTION "IPC library for the GnuPG components"
    URL https://www.gnupg.org/software/libassuan
)
