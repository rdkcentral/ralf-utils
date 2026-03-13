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

#include "W3CPackageMetaDataBuilder.h"
#include "EntosTypes.h"
#include "core/LogMacros.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

static const std::filesystem::path kConfigXmlPath = "config.xml";
static const std::filesystem::path kAppSecretsPath = "appsecrets.json";

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool W3CPackageMetaDataBuilder::isMetaDataFile(const std::filesystem::path &path) const
{
    return (path == kConfigXmlPath) || (path == kAppSecretsPath);
}

bool W3CPackageMetaDataBuilder::addMetaDataFile(const std::filesystem::path &path, std::vector<uint8_t> &&contents)
{
    if (path == kConfigXmlPath)
    {
        m_configXmlContents = std::move(contents);
        return true;
    }
    else if (path == kAppSecretsPath)
    {
        m_appSecretsContents = std::move(contents);
        return true;
    }

    return false;
}

std::shared_ptr<W3CPackageMetaDataImpl> W3CPackageMetaDataBuilder::generate(Error *_Nullable error)
{
    // If already generated the meta-data then just return it
    if (m_metaData)
    {
        return m_metaData;
    }

    // If we don't have the config.xml file then we can't generate the meta-data
    if (m_configXmlContents.empty())
    {
        if (error)
            error->assign(ErrorCode::InvalidPackage, "Package is missing a config.xml file");
        return nullptr;
    }

    // Otherwise parse the config.xml file
    m_metaData = W3CPackageMetaDataImpl::fromConfigXml(m_configXmlContents, error);
    m_configXmlContents.clear();

    // If we have an app secrets file then we can add it to the meta-data as auxiliary meta-data file
    if (!m_appSecretsContents.empty())
    {
        m_metaData->addAuxMetaDataFile(ENTOS_APPSECRETS_MEDIATYPE, std::move(m_appSecretsContents));
    }

    return m_metaData;
}
