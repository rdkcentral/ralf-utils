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


add_library(test-utils

        OBJECT
        TimeUtils.cpp
        FileUtils.cpp
        StringUtils.cpp
        ShaUtils.cpp
        TempDirectory.cpp
)

target_include_directories(test-utils

        PUBLIC
        ..

        PRIVATE
        $<TARGET_PROPERTY:date,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:GTest::gtest_main,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:OpenSSL::Crypto,INTERFACE_INCLUDE_DIRECTORIES>
)


add_library(fake-time-static
        STATIC
        FakeTime.cpp
)

add_library(fake-time-shared
        SHARED
        FakeTime.cpp
)

target_include_directories(fake-time-static PUBLIC .)
target_include_directories(fake-time-shared PUBLIC .)


add_library(mock-file-static
        STATIC
        MockFile.cpp
        MockMount.cpp
)

add_library(mock-file-shared
        SHARED
        MockFile.cpp
        MockMount.cpp
)

target_include_directories(mock-file-static
        PUBLIC
        .
        PRIVATE
        $<TARGET_PROPERTY:GTest::gmock_main,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(mock-file-shared
        PUBLIC
        .
        PRIVATE
        $<TARGET_PROPERTY:GTest::gmock_main,INTERFACE_INCLUDE_DIRECTORIES>)

if (APPLE)
    target_link_options(mock-file-shared PRIVATE "-undefined" "dynamic_lookup")
endif ()
