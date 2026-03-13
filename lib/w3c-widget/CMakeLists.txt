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

add_library(libralf-w3c-widget

        OBJECT
        W3CPackage.cpp
        W3CPackageMetaDataImpl.cpp
        W3CPackageMetaDataBuilder.cpp
        ZipReaderFactory.cpp
)

target_include_directories(libralf-w3c-widget

        PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        $<TARGET_PROPERTY:libralf-core,INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:LibArchive::LibArchive,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(libralf-w3c-widget

        PRIVATE
        LOG_TAG="w3c-widget"
)

if (RALF_UTILS_CLANG_TIDY)
    set_target_properties(libralf-w3c-widget
            PROPERTIES CXX_CLANG_TIDY "${RALF_UTILS_CLANG_TIDY_CMD}")
endif ()

# We use cmake to add the two config.xml schema files to a header to use as cstrings in the code
file(READ ConfigXmlV1.xsd CONFIG_XML_V1_SCHEMA)
file(READ ConfigXmlV2.xsd CONFIG_XML_V2_SCHEMA)
configure_file(ConfigXmlSchemas.h.in ConfigXmlSchemas.h @ONLY)
