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


add_library(libralf-core

        OBJECT
        LibRalf.cpp
        Error.cpp
        Utils.cpp
        Logging.cpp
        Package.cpp
        PackageEntry.cpp
        PackageMetaData.cpp
        PackageAuxMetaData.cpp
        PackageAuxMetaDataImpl.cpp
        PackageReader.cpp
        PackageMount.cpp
        Permissions.cpp
        Base64.cpp
        CryptoDigestBuilder.cpp
        Certificate.cpp
        VerificationBundle.cpp
        DigitalSignature.cpp
        XmlSchemaValidator.cpp
        VersionNumber.cpp
        VersionConstraint.cpp

)

target_include_directories(libralf-core

        PUBLIC
        ..
        ${PROJECT_SOURCE_DIR}/include
        $<TARGET_PROPERTY:libralf-static,BINARY_DIR>

        PRIVATE
        $<TARGET_PROPERTY:LibXml2::LibXml2,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:LibArchive::LibArchive,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(libralf-core

        PRIVATE
        LOG_TAG="core"
)

if (RALF_UTILS_CLANG_TIDY)
    set_target_properties(libralf-core
            PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
endif ()


