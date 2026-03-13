/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Error.h"
#include "LibRalf.h"
#include "W3CPackageMetaDataImpl.h"
#include "core/Compatibility.h"
#include "core/IPackageMetaDataImpl.h"

#include <cinttypes>
#include <filesystem>
#include <memory>
#include <vector>

// -------------------------------------------------------------------------
/*!
    \class W3CPackageMetaDataBuilder
    \brief Internal helper object to create a PackageMetaData object from
    one or more files read from a package.



*/
class W3CPackageMetaDataBuilder
{
public:
    W3CPackageMetaDataBuilder() = default;

    // -------------------------------------------------------------------------
    /*!
        Returns true if the supplied package relative \a path corresponds to
        a known meta data file, ie. "config.xml" or "manifest.json".

    */
    bool isMetaDataFile(const std::filesystem::path &path) const;

    // -------------------------------------------------------------------------
    /*!
        Adds the meta data file contents, this will fail if the file has
        previously been added or the actual file \a path is not a known meta data
        file path.

     */
    bool addMetaDataFile(const std::filesystem::path &path, std::vector<uint8_t> &&contents);

    // -------------------------------------------------------------------------
    /*!
        Populates the ubiquitous PackageMetadata structure with the internally
        stored package meta data files. This will fail and return a nullptr if
        some key files are missing or corrupt (ie. config.xml).

    */
    std::shared_ptr<W3CPackageMetaDataImpl> generate(LIBRALF_NS::Error *_Nullable error);

private:
    std::vector<uint8_t> m_configXmlContents;
    std::vector<uint8_t> m_appSecretsContents;
    std::shared_ptr<W3CPackageMetaDataImpl> m_metaData;
};
