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


add_executable(crypto-test

        TestDataCheck.cpp
        CertificateTests.cpp
        VerificationBundleTests.cpp
        CryptoDigestTests.cpp
        XmlDSigSignatureTests.cpp

        $<TARGET_OBJECTS:test-utils>
        $<TARGET_OBJECTS:libralf-core>
        $<TARGET_OBJECTS:libralf-archive>
        $<TARGET_OBJECTS:libralf-crypto-openssl>
        $<TARGET_OBJECTS:libralf-crypto-xmldsig>

        $<$<BOOL:${RALF_UTILS_ENABLE_W3C_WIDGET}>:$<TARGET_OBJECTS:libralf-w3c-widget>>
        $<$<BOOL:${RALF_UTILS_ENABLE_OCI}>:$<TARGET_OBJECTS:libralf-oci>>
        $<$<BOOL:${RALF_UTILS_ENABLE_OCI}>:$<TARGET_OBJECTS:libralf-dm-verity>>
        $<$<BOOL:${RALF_UTILS_ENABLE_OCI}>:$<TARGET_OBJECTS:libralf-erofs>>
)

get_filename_component(TEST_DATA_DIR "${CMAKE_CURRENT_LIST_DIR}/data/" ABSOLUTE)

target_compile_definitions(crypto-test
        PRIVATE
        TEST_DATA_DIR="${TEST_DATA_DIR}"
)

target_include_directories(crypto-test
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        $<TARGET_PROPERTY:test-utils,INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:libralf-core,INCLUDE_DIRECTORIES>
)

target_link_libraries(crypto-test
        PRIVATE
        fake-time-shared
        OpenSSL::Crypto
        LibXml2::LibXml2
        LibArchive::LibArchive
        lz4::lz4
        GTest::gtest_main
)

gtest_discover_tests(crypto-test)
