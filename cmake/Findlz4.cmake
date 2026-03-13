# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# - Try to find the lz4 library.
#
# The following are set after configuration is done:
#  LZ4_FOUND
#  LZ4_INCLUDE_DIRS
#  LZ4_LIBRARIES
#  LZ4_VERSION

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBLZ4 QUIET liblz4)

find_path(LZ4_INCLUDE_DIR NAMES lz4.h
        HINTS
        ${PC_LIBLZ4_INCLUDEDIR}
        ${PC_LIBLZ4_INCLUDE_DIRS})

find_library(LZ4_LIBRARY NAMES lz4 liblz4
        HINTS
        ${PC_LIBLZ4_LIBDIR}
        ${PC_LIBLZ4_LIBRARY_DIRS})

set(LZ4_VERSION "")
if (PC_LIBLZ4_VERSION)
    set(LZ4_VERSION ${PC_LIBLZ4_VERSION})
elseif (LZ4_INCLUDE_DIR AND EXISTS "${LZ4_INCLUDE_DIR}/lz4.h")
    file(STRINGS "${LZ4_INCLUDE_DIR}/lz4.h" _LZ4_HEADER_VERSION_MAJOR_LINE
            REGEX "#define LZ4_VERSION_MAJOR+[ ]+[0-9]+")
    string(REGEX REPLACE "#define LZ4_VERSION_MAJOR[ ]+([0-9]+).+" "\\1"
            LZ4_VERSION_MAJOR "${_LZ4_HEADER_VERSION_MAJOR_LINE}")
    unset(_LZ4_HEADER_VERSION_MAJOR_LINE)

    file(STRINGS "${LZ4_INCLUDE_DIR}/lz4.h" _LZ4_HEADER_VERSION_MINOR_LINE
            REGEX "#define LZ4_VERSION_MINOR+[ ]+[0-9]+")
    string(REGEX REPLACE "#define LZ4_VERSION_MINOR[ ]+([0-9]+).+" "\\1"
            LZ4_VERSION_MINOR "${_LZ4_HEADER_VERSION_MINOR_LINE}")
    unset(_LZ4_HEADER_VERSION_MINOR_LINE)

    file(STRINGS "${LZ4_INCLUDE_DIR}/lz4.h" _LZ4_HEADER_VERSION_RELEASE_LINE
            REGEX "#define LZ4_VERSION_RELEASE+[ ]+[0-9]+")
    string(REGEX REPLACE "#define LZ4_VERSION_RELEASE[ ]+([0-9]+).+" "\\1"
            LZ4_VERSION_RELEASE "${_LZ4_HEADER_VERSION_RELEASE_LINE}")
    unset(_LZ4_HEADER_VERSION_RELEASE_LINE)

    set(LZ4_VERSION "${LZ4_VERSION_MAJOR}.${LZ4_VERSION_MINOR}.${LZ4_VERSION_RELEASE}")
endif ()

include(FindPackageHandleStandardArgs)

# Handle the QUIETLY and REQUIRED arguments and set the LZ4_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(lz4
        REQUIRED_VARS LZ4_LIBRARY LZ4_INCLUDE_DIR
        VERSION_VAR LZ4_VERSION)

mark_as_advanced(LZ4_INCLUDE_DIR LZ4_LIBRARY)

if (LZ4_FOUND)
    set(LZ4_LIBRARIES ${LZ4_LIBRARY})
    set(LZ4_INCLUDE_DIRS ${LZ4_INCLUDE_DIR})

    if (NOT TARGET lz4::lz4)
        add_library(lz4::lz4 UNKNOWN IMPORTED)
        set_target_properties(lz4::lz4 PROPERTIES
                IMPORTED_LOCATION "${LZ4_LIBRARY}"
                IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                INTERFACE_INCLUDE_DIRECTORIES "${LZ4_INCLUDE_DIRS}")
    endif ()
endif ()
