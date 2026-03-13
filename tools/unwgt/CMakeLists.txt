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

add_executable(unwgt

        unwgt.cpp
        configxml.cpp
        configspec.cpp
)

target_link_libraries(unwgt

        PRIVATE
        libralf-static

        LibXml2::LibXml2
        OpenSSL::Crypto
        LibArchive::LibArchive
        lz4::lz4
)

target_include_directories(unwgt

        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        $<TARGET_PROPERTY:libralf-static,BINARY_DIR>
        $<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
)

if (RALF_UTILS_CLANG_TIDY)
    set_target_properties(unwgt
            PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
endif ()

install(TARGETS unwgt
        DESTINATION bin
)

