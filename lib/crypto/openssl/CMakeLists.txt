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

add_library(libralf-crypto-openssl

        OBJECT
        CryptoUtils.cpp
        Base64Impl.cpp
        CertificateImpl.cpp
        VerificationBundleImpl.cpp
        CryptoDigestBuilderImpl.cpp
        DigitalSignatureImpl.cpp
)

target_include_directories(libralf-crypto-openssl

        PRIVATE
        $<TARGET_PROPERTY:libralf-core,INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:OpenSSL::Crypto,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(libralf-crypto-openssl

        PRIVATE
        LOG_TAG="crypto-openssl"
)

if (RALF_UTILS_CLANG_TIDY)
    set_target_properties(libralf-crypto-openssl
            PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
endif ()
