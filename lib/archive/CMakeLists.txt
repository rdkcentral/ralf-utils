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

add_library(libralf-archive

        OBJECT
        LibarchiveCommon.cpp
        LibarchiveFileReader.cpp
        LibarchiveStreamReader.cpp
        ArchiveEntries.cpp
        ArchiveReader.cpp
)

target_include_directories(libralf-archive

        PUBLIC
        $<TARGET_PROPERTY:LibArchive::LibArchive,INTERFACE_INCLUDE_DIRECTORIES>

        PRIVATE
        $<TARGET_PROPERTY:libralf-core,INCLUDE_DIRECTORIES>
)

target_compile_definitions(libralf-archive

        PRIVATE
        LOG_TAG="archive"
)

if (RALF_UTILS_CLANG_TIDY)
    set_target_properties(libralf-archive
            PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
endif ()
