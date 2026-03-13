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


# TODO: if using vcpkg for the build, use gtest / gmock from that rather than fetching directly
include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/f3c355f9dd382bc2c323be2713e351a578b68c61.zip
        URL_HASH SHA256=f181f31ad160e3b768f4d4d4fed6d8e12de4a1a3f4aa0a99aa078a09894d54c9
)
FetchContent_MakeAvailable(googletest)


FetchContent_Declare(
        date
        URL https://github.com/HowardHinnant/date/archive/refs/tags/v3.0.3.zip
        URL_HASH SHA256=cf60e95c9974a7bebea64b1deb4cef281382d9ed0d95dde32f8277d32b636d53
)
FetchContent_MakeAvailable(date)


enable_testing()

include(GoogleTest)

add_subdirectory(utils)

add_subdirectory(core)
add_subdirectory(crypto)
add_subdirectory(dm-verity)
if (RALF_UTILS_ENABLE_EROFS)
    add_subdirectory(erofs)
endif ()

if (RALF_UTILS_CLANG_TIDY)
    set_target_properties(test-utils PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
    set_target_properties(core-test PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
    set_target_properties(crypto-test PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
    set_target_properties(dmverity-test PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
    if (RALF_UTILS_ENABLE_EROFS)
        set_target_properties(erofs-test PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
    endif ()
endif ()
