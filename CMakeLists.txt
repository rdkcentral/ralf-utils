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

cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

project(ralf-utils
        VERSION 1.2.0
        LANGUAGES C CXX
        DESCRIPTION "Library and tool for unpacking and analysing widget and RALF package files")

# Build as a shared library by default, this can be overridden by the user
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# Option to enable AddressSanitiser for the libralf build, this is a debugging feature to capture common memory
# errors and leaks.  Disabled by default.
# \see https://clang.llvm.org/docs/AddressSanitizer.html
option(RALF_UTILS_ENABLE_ADDRESS_SANITIZER "Enable AddressSanitizer" OFF)

# Option to enable UndefinedBehaviorSanitizer for the libralf build, this is a debugging feature to capture common
# various kinds of undefined behavior during program execution.  Disabled by default.
# \see https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
option(RALF_UTILS_ENABLE_UNDEFINED_BEHAVIOUR_SANITIZER "Enable UndefinedBehaviorSanitizer" OFF)

# Enables support for reading and verify EROFS formatted packages
option(RALF_UTILS_ENABLE_EROFS "Enable EROFS support" ON)

# Enables support for reading and verify W3C formatted packages
option(RALF_UTILS_ENABLE_W3C_WIDGET "Enable W3C Widget support" ON)

# Enables support for reading and verify OCI formatted packages
option(RALF_UTILS_ENABLE_OCI "Enable OCI Package support" ON)

# Option to enable building unit tests, adds a build dependency on gtest / gmock
option(RALF_UTILS_BUILD_UNIT_TESTS "Built all unit tests" OFF)

# Option to enable building the tools, these are command line utilities to interact with the library
option(RALF_UTILS_BUILD_TOOLS "Built all tools" ON)

# Use an external version of nlohmann_json instead of the one bundled with the project
option(RALF_UTILS_USE_EXTERNAL_JSON "Use an external JSON library" OFF)

# Option to enable clang-tidy on the source files
option(RALF_UTILS_CLANG_TIDY "Run clang-tidy on source files" OFF)
if (RALF_UTILS_CLANG_TIDY)
    if (NOT CLANG_TIDY_EXE)
        find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
    endif()
    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR "clang-tidy not found")
    endif()

    set(RALF_UTILS_CLANG_TIDY_CMD "${CLANG_TIDY_EXE};--use-color;-warnings-as-errors=*")
endif()


# Fall back to our own FindXXX.cmake files if the system ones don't exist
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Find required packages
find_package(LibXml2 REQUIRED)
find_package(LibArchive REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(lz4 REQUIRED)

# Due to bug https://github.com/lz4/lz4/issues/783 in liblz4 we need to ensure that we have at least version 1.9.3
# otherwise can hit issues with extracting legacy EROFS images that uses LZ4 compression.
if (LZ4_VERSION VERSION_LESS 1.9.3)
    message(FATAL_ERROR "liblz4 version 1.9.3 or higher is required, found version ${LZ4_VERSION}")
endif()


# Older versions of FindLibArchive.cmake don't have the LibArchive::LibArchive target so add it manually
if (NOT TARGET LibArchive::LibArchive)
    add_library(LibArchive::LibArchive INTERFACE IMPORTED)
    set_target_properties(LibArchive::LibArchive PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LibArchive_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${LibArchive_LIBRARIES}")
endif ()


# We require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Lets be ambitious and eliminate all warnings (not that ambitious to enable -Werror though)
add_compile_options(-Wall)

# The following disables an annoying warning "<foo> will change in GCC X.XX"
add_compile_options(-Wno-psabi)

# Export the project version to code
set(RALF_UTILS_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(RALF_UTILS_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(RALF_UTILS_VERSION_MICRO ${PROJECT_VERSION_PATCH})
set(RALF_UTILS_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

add_compile_definitions(
        RALF_UTILS_VERSION_MAJOR=${RALF_UTILS_VERSION_MAJOR}
        RALF_UTILS_VERSION_MINOR=${RALF_UTILS_VERSION_MINOR}
        RALF_UTILS_VERSION_MICRO=${RALF_UTILS_VERSION_MICRO}
        RALF_UTILS_VERSION_STRING="${RALF_UTILS_VERSION_STRING}"
)

# Build position independent code by default, this is required for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable AddressSanitizer if requested
if (RALF_UTILS_ENABLE_ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

# Add any third party libraries we need
add_subdirectory(thirdparty)

# Add the library to the build
add_subdirectory(lib)

# Add the tools to the build
if (RALF_UTILS_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Add the tests to the build
if (RALF_UTILS_BUILD_UNIT_TESTS)
    add_subdirectory(test)
endif()

