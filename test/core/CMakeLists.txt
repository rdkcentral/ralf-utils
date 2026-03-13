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


add_executable(core-test

        TestDataCheck.cpp
        ErrorTests.cpp
        VersionNumberTests.cpp
        VersionConstraintTest.cpp
        W3CPackageMetaDataTests.cpp
        Base64Tests.cpp
        W3CPackageTests.cpp
        OCIPackageConfigTests.cpp
        MetaDataJsonTests.cpp
        PackageIdTests.cpp
        ArchiveTests.cpp
        PackageExtractionTests.cpp
        PackageFormatTests.cpp
        PackageMountableTests.cpp
        PackageVerificationTests.cpp

        $<TARGET_OBJECTS:test-utils>

        $<TARGET_OBJECTS:libralf-core>
        $<TARGET_OBJECTS:libralf-archive>
        $<TARGET_OBJECTS:libralf-oci>
        $<TARGET_OBJECTS:libralf-erofs>
        $<TARGET_OBJECTS:libralf-dm-verity>
        $<TARGET_OBJECTS:libralf-w3c-widget>
        $<TARGET_OBJECTS:libralf-crypto-openssl>
        $<TARGET_OBJECTS:libralf-crypto-xmldsig>
)

target_include_directories(core-test
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        $<TARGET_PROPERTY:libralf-core,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:libralf-archive,INTERFACE_INCLUDE_DIRECTORIES>

        $<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
)

get_filename_component(TEST_DATA_DIR "${CMAKE_CURRENT_LIST_DIR}/data/" ABSOLUTE)

target_compile_definitions(core-test
        PRIVATE
        TEST_DATA_DIR="${TEST_DATA_DIR}"
)

target_link_libraries(core-test
        PRIVATE
        fake-time-shared
        OpenSSL::Crypto
        LibArchive::LibArchive
        LibXml2::LibXml2
        lz4::lz4
        GTest::gmock_main
)

gtest_discover_tests(core-test)
