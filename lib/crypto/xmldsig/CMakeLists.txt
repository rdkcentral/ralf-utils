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

add_library(libralf-crypto-xmldsig

        OBJECT
        XmlDigitalSignature.cpp
        XmlDigitalSignatureImpl.cpp
        XmlDSigUtils.cpp
)

target_include_directories(libralf-crypto-xmldsig

        PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        $<TARGET_PROPERTY:libralf-core,INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:LibXml2::LibXml2,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(libralf-crypto-xmldsig

        PRIVATE
        LOG_TAG="crypto-xmldsig"
)

if (RALF_UTILS_CLANG_TIDY)
    set_target_properties(libralf-crypto-xmldsig
            PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
endif ()

# We use cmake to add the Signature1.xsd schema file to a header to use as cstrings in the code
file(READ Signature1.xsd XMLDSIG_SCHEMA)
configure_file(XmlDSigSchema.h.in XmlDSigSchema.h @ONLY)

# We currently use OpenSSL to perform the base64 decoding of the strings in the signature.  Ideally this
# code should be completely separate of the crypto library, but for now I didn't want to include another
# OSS library or code snippet for performing base64 decodes.  In the future I hope to remove this
# requirement from the xmldsig part of this library
if (OPENSSL_FOUND)
    target_include_directories(libralf-crypto-xmldsig PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_compile_definitions(libralf-crypto-xmldsig PRIVATE USE_OPENSSL_BASE64_DECODE=1)
endif ()

