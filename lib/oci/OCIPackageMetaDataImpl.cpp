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

#include "OCIPackageMetaDataImpl.h"
#include "EntosTypes.h"
#include "VersionNumber.h"
#include "core/LogMacros.h"
#include "core/Utils.h"

#include <sstream>
#include <string_view>
#include <vector>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace std::string_view_literals;

Result<std::shared_ptr<OCIPackageMetaDataImpl>> OCIPackageMetaDataImpl::fromConfigJson(const std::vector<uint8_t> &contents)
{
    const auto json = nlohmann::json::parse(contents, nullptr, false);
    if (json.is_discarded())
        return Error(ErrorCode::PackageContentsInvalid, "Failed to parse package config JSON");
    if (!json.is_object())
        return Error(ErrorCode::PackageContentsInvalid, "Failed to parse package config JSON - not an object");

    auto metaDataImpl = std::make_shared<OCIPackageMetaDataImpl>();

    try
    {
        // Nb: we don't do a JSON schema check here ... so it's a manual check

        static const std::vector<std::pair<std::string_view, ProcessFieldFunction>> kRequiredFields = {
            { "id"sv, std::mem_fn(&OCIPackageMetaDataImpl::processId) },
            { "version"sv, std::mem_fn(&OCIPackageMetaDataImpl::processVersion) },
            { "packageType"sv, std::mem_fn(&OCIPackageMetaDataImpl::processPackageType) },
            { "entryPoint"sv, std::mem_fn(&OCIPackageMetaDataImpl::processEntryPoint) },
        };

        for (const auto &[key, processField] : kRequiredFields)
        {
            if (!json.contains(key))
                return Error::format(ErrorCode::PackageContentsInvalid,
                                     "Failed to parse package config JSON - missing '%s'", key.data());
            if (!processField(metaDataImpl.get(), json.at(key)))
                return Error::format(ErrorCode::PackageContentsInvalid,
                                     "Failed to parse package config JSON - invalid '%s'", key.data());
        }

        static const std::vector<std::pair<std::string_view, ProcessFieldFunction>> kOptionalFields = {
            { "name"sv, std::mem_fn(&OCIPackageMetaDataImpl::processName) },
            { "versionName"sv, std::mem_fn(&OCIPackageMetaDataImpl::processVersionName) },
            { "entryArgs"sv, std::mem_fn(&OCIPackageMetaDataImpl::processEntryArgs) },
            { "packageSpecifier"sv, std::mem_fn(&OCIPackageMetaDataImpl::processPackageSpecifier) },
            { "dependencies"sv, std::mem_fn(&OCIPackageMetaDataImpl::processDependencies) },
            { "permissions"sv, std::mem_fn(&OCIPackageMetaDataImpl::processPermissions) },
            { "configuration"sv, std::mem_fn(&OCIPackageMetaDataImpl::processConfiguration) },
        };

        for (const auto &[key, processField] : kOptionalFields)
        {
            if (json.contains(key) && !processField(metaDataImpl.get(), json.at(key)))
                return Error::format(ErrorCode::PackageContentsInvalid,
                                     "Failed to parse package config JSON - invalid '%s'", key.data());
        }

        // Set the mime type based on the package type and optional specifier
        switch (metaDataImpl->m_type)
        {
            case PackageType::Application:
                metaDataImpl->m_mimeType = "application/";
                break;
            case PackageType::Runtime:
                metaDataImpl->m_mimeType = "runtime/";
                break;
            case PackageType::Service:
                metaDataImpl->m_mimeType = "service/";
                break;
            case PackageType::Base:
                metaDataImpl->m_mimeType = "base/";
                break;
            case PackageType::Resource:
                metaDataImpl->m_mimeType = "resource/";
                break;
            default:
                return Error::format(ErrorCode::PackageContentsInvalid, "Unknown package type");
        }

        if (!metaDataImpl->m_specifier.empty())
            metaDataImpl->m_mimeType.append(metaDataImpl->m_specifier);
        else
            metaDataImpl->m_mimeType.append("unknown");
    }
    catch (const std::exception &e)
    {
        return Error::format(ErrorCode::PackageContentsInvalid, "Failed to process config.json - %s", e.what());
    }

    return metaDataImpl;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Processes the 'id' field in the package config JSON.  The id follows the
    same rules as for widgets.
 */
bool OCIPackageMetaDataImpl::processId(const nlohmann::json &json)
{
    if (!json.is_string())
        return false;

    std::string id = json.get<std::string>();
    if (!verifyPackageId(id))
        return false;

    m_id = std::move(id);
    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Processes the 'version' field which is a strict semver.
 */
bool OCIPackageMetaDataImpl::processVersion(const nlohmann::json &json)
{
    if (!json.is_string())
        return false;

    auto ver = VersionNumber::fromString(json.get<std::string>());
    if (!ver)
        return false;

    m_version = ver.value();
    return true;
}

bool OCIPackageMetaDataImpl::processVersionName(const nlohmann::json &json)
{
    if (!json.is_string())
        return false;

    m_versionName = json.get<std::string>();
    return !m_versionName.empty();
}

bool OCIPackageMetaDataImpl::processPackageType(const nlohmann::json &json)
{
    if (!json.is_string())
        return false;

    const auto &value = json.get_ref<const std::string &>();
    if (strcasecmp(value.c_str(), "application") == 0)
        m_type = PackageType::Application;
    else if (strcasecmp(value.c_str(), "runtime") == 0)
        m_type = PackageType::Runtime;
    else if (strcasecmp(value.c_str(), "service") == 0)
        m_type = PackageType::Service;
    else if (strcasecmp(value.c_str(), "base") == 0)
        m_type = PackageType::Base;
    else if (strcasecmp(value.c_str(), "resource") == 0)
        m_type = PackageType::Resource;
    else
        return false;

    return true;
}

bool OCIPackageMetaDataImpl::processPackageSpecifier(const nlohmann::json &json)
{
    if (!json.is_string())
        return false;

    m_specifier = json.get<std::string>();
    if (m_specifier.empty())
        return false;

    return true;
}

bool OCIPackageMetaDataImpl::processName(const nlohmann::json &json)
{
    if (!json.is_string())
        return false;

    auto title = json.get<std::string>();
    if (title.empty())
        return false;

    m_title = std::move(title);
    return true;
}

bool OCIPackageMetaDataImpl::processEntryPoint(const nlohmann::json &json)
{
    if (!json.is_string())
        return false;

    std::filesystem::path entryPoint = json.get<std::string>();
    if (entryPoint.empty())
        return false;

    m_entryPointPath = std::move(entryPoint);
    return true;
}

bool OCIPackageMetaDataImpl::processEntryArgs(const nlohmann::json &json)
{
    if (!json.is_array())
        return false;

    m_entryArgs.clear();

    for (const auto &arg : json)
    {
        if (!arg.is_string())
            return false;

        m_entryArgs.emplace_back(arg.get<std::string>());
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Processes the dependencies map / list.  The dependencies is a JSON
    object with the package id as the key and a version constraint as the
    value.

 */
bool OCIPackageMetaDataImpl::processDependencies(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    m_dependencies.clear();

    for (const auto &[id, constraint] : json.items())
    {
        if (!constraint.is_string())
            return false;

        if (!verifyPackageId(id))
            return false;

        auto verConstraint = VersionConstraint::fromString(constraint.get<std::string>());
        if (!verConstraint)
            return false;

        m_dependencies.emplace(id, verConstraint.value());
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Processes the list of icons.  Each icon entry is an object with a
    mandatory 'src' field and optional `sizes`, `type` and `purpose` fields.

    Icons were removed from the official part of the metadata spec, which
    I think was a mistake, so for now they are kept as EntOS specific
    configuration.

    Longer term we maybe should look at storing the icon in a separate
    layer, or keeping it in the image layer, but referencing it in the
    package manifest as an annotation or something similar.

 */
bool OCIPackageMetaDataImpl::processIconsConfig(const nlohmann::json &json)
{
    if (!json.is_array())
        return false;

    m_icons.clear();

    for (const auto &icon : json)
    {
        if (!icon.is_object())
            return false;

        std::filesystem::path src = icon.value("src", "");
        if (src.empty())
        {
            logWarning("Icon entry is missing 'src' field or it's not a string");
            continue;
        }
        if (!verifyPackagePath(src))
        {
            logWarning("Icon entry 'src' field is not a valid path");
            continue;
        }

        Icon iconData;
        iconData.path = std::move(src);
        iconData.mimeType = icon.value("type", "");
        iconData.purpose = icon.value("purpose", "");

        // sizes is a string with one or more fields with width x height separated by a space, e.g. "16x16 32x32"
        std::string sizes = icon.value("sizes", "");
        if (!sizes.empty())
        {
            std::istringstream stream(sizes);
            std::string size;
            while (std::getline(stream, size, ' '))
            {
                auto xPos = size.find('x');
                if (xPos != std::string::npos)
                {
                    int width = std::stoi(size.substr(0, xPos));
                    int height = std::stoi(size.substr(xPos + 1));
                    iconData.sizes.emplace_back(width, height);
                }
            }
        }

        // Add the icon to the list
        m_icons.emplace_back(std::move(iconData));
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Processes the capability list, this is just a string list and the only
    sanity check we do is ensure the string is not empty.

 */
bool OCIPackageMetaDataImpl::processPermissions(const nlohmann::json &json)
{
    if (!json.is_array())
        return false;

    for (const auto &perm : json)
    {
        if (!perm.is_string())
            return false;

        std::string cap = perm.get<std::string>();
        if (cap.empty())
            return false;

        m_permissions->set(cap, true);
    }

    return true;
}

bool OCIPackageMetaDataImpl::processConfiguration(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    static const std::map<std::string_view, ProcessFieldFunction> kSettingsMap = {
        { ENTOS_ICONS_CONFIGURATION ""sv, &OCIPackageMetaDataImpl::processIconsConfig },
        { "urn:rdk:config:memory"sv, &OCIPackageMetaDataImpl::processMemoryConfig },
        { "urn:rdk:config:storage"sv, &OCIPackageMetaDataImpl::processStorageConfig },
        { "urn:rdk:config:dial"sv, &OCIPackageMetaDataImpl::processDialConfig },
        { "urn:rdk:config:network"sv, &OCIPackageMetaDataImpl::processNetworkConfig },
        { "urn:rdk:config:log-levels"sv, &OCIPackageMetaDataImpl::processLogLevelsConfig },
        { "urn:rdk:config:application-lifecycle"sv, &OCIPackageMetaDataImpl::processAppLifecycleConfig },
        { ENTOS_LOW_POWER_TERMINATE_CONFIGURATION ""sv, &OCIPackageMetaDataImpl::processLowPowerTerminateConfig },
        { "urn:rdk:config:input-handling"sv, &OCIPackageMetaDataImpl::processInputHandlingConfig },
        { "urn:rdk:config:window"sv, &OCIPackageMetaDataImpl::processWindowConfig },
        { ENTOS_DISPLAY_CONFIGURATION ""sv, &OCIPackageMetaDataImpl::processDisplayConfig },
        { ENTOS_AUDIO_CONFIGURATION ""sv, &OCIPackageMetaDataImpl::processAudioConfig },
        { "urn:rdk:config:overrides"sv, &OCIPackageMetaDataImpl::processOverridesConfig },
    };

    for (const auto &[key, value] : json.items())
    {
        // We store all settings in a map so they can be retrieved by the `vendorSetting` method
        m_allConfigs.emplace(key, value.get<JSON>());

        // Check if the setting is known and process it
        auto it = kSettingsMap.find(key);
        if (it != kSettingsMap.end())
        {
            if (!it->second(this, value))
            {
                logError("Failed to process setting '%s' in package config JSON", key.c_str());
                return false;
            }
        }
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        "urn:rdk:config:dial": {
            "appNames": ["MyMediaApp", "MediaRemote"],
            "corsDomains": ["http://example.com", "https://my-media.com"],
            "originHeaderRequired": true
        }
    \endcode

 */
bool OCIPackageMetaDataImpl::processDialConfig(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    DialInfo dialInfo = {};

    for (const auto &[key, value] : json.items())
    {
        if ((key == "appNames") && value.is_array())
            dialInfo.dialIds = value.get<std::vector<std::string>>();
        else if ((key == "corsDomains") && value.is_array())
            dialInfo.corsDomains = value.get<std::vector<std::string>>();
        else if ((key == "originHeaderRequired") && value.is_boolean())
            dialInfo.originHeaderRequired = value.get<bool>();
        else
            logWarning("Unknown DIAL config key '%s' in package config JSON, ignoring", key.c_str());
    }

    m_dialInfo = std::move(dialInfo);
    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        "urn:rdk:config:log-levels": [
            "error",
            "warning",
            "milestone"
        ]
    \endcode

 */
bool OCIPackageMetaDataImpl::processLogLevelsConfig(const nlohmann::json &json)
{
    if (!json.is_array())
        return false;

    m_loggingLevels = LoggingLevel::Default;

    for (const auto &level : json)
    {
        if (!level.is_string())
            return false;

        const std::string levelStr = level.get<std::string>();
        if (strcasecmp(levelStr.c_str(), "fatal") == 0)
            m_loggingLevels |= LoggingLevel::Fatal;
        else if (strcasecmp(levelStr.c_str(), "error") == 0)
            m_loggingLevels |= LoggingLevel::Error;
        else if (strcasecmp(levelStr.c_str(), "warning") == 0)
            m_loggingLevels |= LoggingLevel::Warning;
        else if (strcasecmp(levelStr.c_str(), "milestone") == 0)
            m_loggingLevels |= LoggingLevel::Milestone;
        else if (strcasecmp(levelStr.c_str(), "info") == 0)
            m_loggingLevels |= LoggingLevel::Info;
        else if (strcasecmp(levelStr.c_str(), "debug") == 0)
            m_loggingLevels |= LoggingLevel::Debug;
        else
            logWarning("Unknown logging level '%s' in package config JSON, ignoring", levelStr.c_str());
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        {
            "configuration": {
                "urn:rdk:config:network": [
                    {
                        "name": "netflix-mdx",
                        "port": 8009,
                        "protocol": "tcp",
                        "type": "public"
                    },
                    {
                        "name": "com.example.myapp.service",
                        "port": 1234,
                        "protocol": "tcp",
                        "type": "exported"
                    },
                    {
                        "name": "com.example.someotherapp.service",
                        "port": 4567,
                        "protocol": "tcp",
                        "type": "imported"
                    }
                ]
            }
        }
    \endcode

 */
bool OCIPackageMetaDataImpl::processNetworkConfig(const nlohmann::json &json)
{
    if (!json.is_array())
        return false;

    enum class NetworkServiceType
    {
        Unknown,
        Public,
        Exported,
        Imported
    };

    for (const auto &entry : json)
    {
        if (!entry.is_object())
        {
            logWarning("Network service entry is not an object in package config JSON");
            return false;
        }

        NetworkService netService = {};
        NetworkServiceType type = NetworkServiceType::Unknown;
        for (const auto &[key, value] : entry.items())
        {
            if ((key == "name") && value.is_string())
            {
                netService.name = value.get<std::string>();
            }
            else if ((key == "port") && value.is_number_integer())
            {
                netService.port = value.get<uint16_t>();
            }
            else if ((key == "protocol") && value.is_string())
            {
                const auto &protoStr = value.get_ref<const std::string &>();
                if (strcasecmp(protoStr.c_str(), "tcp") == 0)
                    netService.protocol = NetworkService::Protocol::TCP;
                else if (strcasecmp(protoStr.c_str(), "udp") == 0)
                    netService.protocol = NetworkService::Protocol::UDP;
                else
                    logWarning("Unknown protocol '%s' in package config JSON, defaulting to TCP", protoStr.c_str());
            }
            else if ((key == "type") && value.is_string())
            {
                const auto &typeStr = value.get_ref<const std::string &>();
                if (strcasecmp(typeStr.c_str(), "public") == 0)
                    type = NetworkServiceType::Public;
                else if (strcasecmp(typeStr.c_str(), "exported") == 0)
                    type = NetworkServiceType::Exported;
                else if (strcasecmp(typeStr.c_str(), "imported") == 0)
                    type = NetworkServiceType::Imported;
                else
                    logWarning("Unknown network service type '%s' in package config JSON", typeStr.c_str());
            }
        }

        if ((netService.port == 0) || (type == NetworkServiceType::Unknown))
        {
            logWarning("Network service entry is missing valid 'port' or 'type' in package config JSON");
        }
        else
        {
            if (type == NetworkServiceType::Exported)
                m_exportedServices.emplace_back(std::move(netService));
            else if (type == NetworkServiceType::Imported)
                m_importedServices.emplace_back(std::move(netService));
            else if (type == NetworkServiceType::Public)
                m_publicServices.emplace_back(std::move(netService));
        }
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        "urn:rdk:config:application-lifecycle": {
            "supportedNonActiveStates": ["paused", "suspended", "hibernated"],
            "maxSuspendedSystemMemory": "16M",
            "maxTimeToSuspendMemoryState": 10,
            "startupTimeout": 60,
            "watchdogInterval": 30
        }
    \endcode

 */
bool OCIPackageMetaDataImpl::processAppLifecycleConfig(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    for (const auto &[key, value] : json.items())
    {
        if ((key == "supportedNonActiveStates") && value.is_array())
        {
            for (const auto &state : value)
            {
                if (!state.is_string())
                    continue;

                // FIXME: these state strings don't really align with both existing code, firebolt lifecycle 1.0 and 2.0
                const auto &stateStr = state.get_ref<const std::string &>();
                if (strcasecmp(stateStr.c_str(), "paused") == 0)
                    m_supportedLifecycleStates |= LifecycleState::Paused;
                else if (strcasecmp(stateStr.c_str(), "suspended") == 0)
                    m_supportedLifecycleStates |= LifecycleState::Suspended;
                else if (strcasecmp(stateStr.c_str(), "hibernated") == 0)
                    m_supportedLifecycleStates |= LifecycleState::Hibernated;
            }
        }
        else if ((key == "maxSuspendedSystemMemory") && value.is_string())
        { // NOLINT
            // FIXME: this is currently not exposed in the API, but we should add it
        }
        else if ((key == "maxTimeToSuspendMemoryState") && value.is_number())
        { // NOLINT
            // FIXME: this is also currently not exposed in the API, but we should add it
        }
        else if ((key == "startupTimeout") && value.is_number())
        {
            const float seconds = value.get<float>();
            if (seconds < 0.0f)
            {
                logWarning("Startup timeout is a negative value, ignoring");
            }
            else
            {
                m_startTimeout = std::chrono::milliseconds(static_cast<unsigned long>(seconds * 1000.0f));
            }
        }
        else if ((key == "watchdogInterval") && value.is_number())
        {
            const float seconds = value.get<float>();
            if (seconds < 0.0f)
            {
                logWarning("Watchdog interval is a negative value, ignoring");
            }
            else
            {
                m_watchdogInterval = std::chrono::milliseconds(static_cast<unsigned long>(seconds * 1000.0f));
            }
        }
        else
        {
            logWarning("Unknown application lifecycle config key '%s' in package config JSON", key.c_str());
        }
    }

    return true;
}

bool OCIPackageMetaDataImpl::processLowPowerTerminateConfig(const nlohmann::json &json)
{
    if (!json.is_boolean())
        return false;

    if (json.get<bool>())
        m_supportedLifecycleStates &= ~LifecycleState::LowPower;
    else
        m_supportedLifecycleStates |= LifecycleState::LowPower;

    return true;
}

std::optional<uint64_t> OCIPackageMetaDataImpl::parseMemorySize(const std::string &str)
{
    if (str.empty())
        return std::nullopt;

    const char *ptr = str.c_str();
    char *endPtr = nullptr;

    uint64_t value = strtoull(ptr, &endPtr, 0);

    if (endPtr == ptr)
        return std::nullopt;

    if (endPtr)
    {
        switch (*endPtr)
        {
            case 'K':
            case 'k':
                return value * 1024;
            case 'M':
            case 'm':
                return value * 1024 * 1024;
            case 'G':
            case 'g':
                return value * 1024 * 1024 * 1024;
            default:
                return value;
        }
    }

    return value;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        {
            "configuration": {
                "urn:rdk:config:storage": {
                    "maxLocalStorage": "32M",
                    "sharedStorageAppId": "com.sky.myapp2"
                }
            }
        }
    \endcode

    \see https://github.com/rdkcentral/oci-package-spec/blob/main/metadata.md#urnrdkconfigstorage
 */
bool OCIPackageMetaDataImpl::processStorageConfig(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    for (const auto &[key, value] : json.items())
    {
        if (key == "maxLocalStorage")
        {
            if (!value.is_string())
                return false;

            m_storageQuota = parseMemorySize(value.get_ref<const std::string &>());
        }
        else if (key == "sharedStorageAppId")
        {
            if (!value.is_string())
                return false;

            const auto &appId = value.get_ref<const std::string &>();
            if (!verifyPackageId(appId))
                return false;

            m_storageGroup = appId;
        }
        else
        {
            logWarning("Unknown storage configuration key '%s' in package config JSON", key.c_str());
        }
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        "configuration": {
            "urn:rdk:config:memory": {
                "system": "256M",
                "gpu": "128M"
            }
        }
    \endcode

 */
bool OCIPackageMetaDataImpl::processMemoryConfig(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    for (const auto &[key, value] : json.items())
    {
        if ((key == "system") && value.is_string())
            m_memoryQuota = parseMemorySize(value.get_ref<const std::string &>());
        else if ((key == "gpu") && value.is_string())
            m_gpuMemoryQuota = parseMemorySize(value.get_ref<const std::string &>());
        else
            logWarning("Unknown memory configuration key '%s' in package config JSON", key.c_str());
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        "configuration": {
            "urn:rdk:config:input-handling": {
                "keyIntercept": ["search", "voice"],
                "keyCapture": ["search", "voice"],
                "keyMonitor": ["volume+", "volume-"]
            }
        }
    \endcode

 */
bool OCIPackageMetaDataImpl::processInputHandlingConfig(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    InputHandlingInfo inputHandling = {};

    for (const auto &[key, value] : json.items())
    {
        if ((key == "keyIntercept") && value.is_array())
        {
            for (const auto &item : value)
            {
                if (item.is_string())
                    inputHandling.interceptedKeys.emplace_back(item.get<std::string>());
            }
        }
        else if ((key == "keyCapture") && value.is_array())
        {
            for (const auto &item : value)
            {
                if (item.is_string())
                    inputHandling.capturedKeys.emplace_back(item.get<std::string>());
            }
        }
        else if ((key == "keyMonitor") && value.is_array())
        {
            for (const auto &item : value)
            {
                if (item.is_string())
                    inputHandling.monitoredKeys.emplace_back(item.get<std::string>());
            }
        }
        else
        {
            logWarning("Unknown input-handling configuration key '%s' in package config JSON", key.c_str());
        }
    }

    if (!inputHandling.interceptedKeys.empty() || !inputHandling.monitoredKeys.empty() ||
        !inputHandling.capturedKeys.empty())
    {
        m_inputHandlingInfo = inputHandling;
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        "configuration": {
            "urn:rdk:config:window": {
                "virtualDisplaySize": 1080
            }
        }
    \endcode

 */
bool OCIPackageMetaDataImpl::processWindowConfig(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    for (const auto &[key, value] : json.items())
    {
        if ((key == "virtualDisplaySize") && value.is_number())
        {
            switch (value.get<int>())
            {
                case 480:
                    m_displayInfo.size = DisplaySize::Size720x480;
                    break;
                case 576:
                    m_displayInfo.size = DisplaySize::Size720x576;
                    break;
                case 720:
                    m_displayInfo.size = DisplaySize::Size1280x720;
                    break;
                case 1080:
                    m_displayInfo.size = DisplaySize::Size1920x1080;
                    break;
                case 2160:
                    m_displayInfo.size = DisplaySize::Size3840x2160;
                    break;
                case 4320:
                    m_displayInfo.size = DisplaySize::Size7680x4320;
                    break;
                default:
                    logWarning("Unsupported virtual display size '%d' in package config JSON", value.get<int>());
                    break;
            }
        }
        else
        {
            logWarning("Unknown window configuration key '%s' in package config JSON", key.c_str());
        }
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        "configuration": {
            "urn:entos:config:display": {
                "refreshRateHz": 60,
                "pictureMode": "Cinema"
            }
        }
    \endcode

 */
bool OCIPackageMetaDataImpl::processDisplayConfig(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    for (const auto &[key, value] : json.items())
    {
        if ((key == "refreshRateHz") && value.is_number())
        {
            switch (value.get<int>())
            {
                case 50:
                    m_displayInfo.refreshRate = DisplayRefreshRate::FiftyHz;
                    break;
                case 60:
                    m_displayInfo.refreshRate = DisplayRefreshRate::SixtyHz;
                    break;
                default:
                    logWarning("Unsupported display refresh rate '%d' in package config JSON", value.get<int>());
                    break;
            }
        }
        else if ((key == "pictureMode") && value.is_string())
        {
            m_displayInfo.pictureMode = value.get<std::string>();
        }
        else
        {
            logWarning("Unknown display configuration key '%s' in package config JSON", key.c_str());
        }
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        "configuration": {
            "urn:entos:config:audio": {
                "soundMode": "<name of sound mode>",
                "soundScene": "<name of sound scene>",
                "loudnessAdjustment": <integer value in dB>
            }
        }
    \endcode

 */
bool OCIPackageMetaDataImpl::processAudioConfig(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    for (const auto &[key, value] : json.items())
    {
        if ((key == "soundMode") && value.is_string())
        {
            m_audioInfo.soundMode = value.get<std::string>();
        }
        else if ((key == "soundScene") && value.is_string())
        {
            m_audioInfo.soundScene = value.get<std::string>();
        }
        else if ((key == "loudnessAdjustment") && value.is_number_integer())
        {
            m_audioInfo.loudnessAdjustment = value.get<int>();
        }
        else
        {
            logWarning("Unknown audio configuration key '%s' in package config JSON", key.c_str());
        }
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    \code
        "configuration": {
            "urn:rdk:config:overrides": {
                "application": {
                    "colorTheme": "vivid"
                },
                "runtime": {
                    "userAgent": "BBC_requirement_requesting_Operator_Specific_UserAgent",
                    "libInject": "oiptv.js",
                    "mTls": "/etc/certs/client_operator.cert"
                }
            }
        }
    \endcode

 */
bool OCIPackageMetaDataImpl::processOverridesConfig(const nlohmann::json &json)
{
    if (!json.is_object())
        return false;

    for (const auto &[key, value] : json.items())
    {
        if (key == "application")
        {
            m_overrides[Override::Application] = value.get<JSON>();
        }
        else if (key == "runtime")
        {
            m_overrides[Override::Runtime] = value.get<JSON>();
        }
        else if (key == "base")
        {
            m_overrides[Override::Base] = value.get<JSON>();
        }
        else
        {
            logWarning("Unknown overrides configuration key '%s' in package config JSON", key.c_str());
        }
    }

    return true;
}

OCIPackageMetaDataImpl::OCIPackageMetaDataImpl()
    : m_permissions(std::make_shared<PermissionsImpl>())
{
}

const std::string &OCIPackageMetaDataImpl::id() const
{
    return m_id;
}

VersionNumber OCIPackageMetaDataImpl::version() const
{
    return m_version;
}

const std::string &OCIPackageMetaDataImpl::versionName() const
{
    return m_versionName;
}

PackageType OCIPackageMetaDataImpl::type() const
{
    return m_type;
}

const std::string &OCIPackageMetaDataImpl::mimeType() const
{
    return m_mimeType;
}

const std::string &OCIPackageMetaDataImpl::runtimeType() const
{
    return m_specifier;
}

const std::optional<std::string> &OCIPackageMetaDataImpl::title() const
{
    return m_title;
}

const PlatformInfo &OCIPackageMetaDataImpl::platformInfo() const
{
    return m_platformInfo;
}

const std::filesystem::path &OCIPackageMetaDataImpl::entryPointPath() const
{
    return m_entryPointPath;
}

const std::list<std::string> &OCIPackageMetaDataImpl::entryArgs() const
{
    return m_entryArgs;
}

const std::map<std::string, VersionConstraint> &OCIPackageMetaDataImpl::dependencies() const
{
    return m_dependencies;
}

const std::list<Icon> &OCIPackageMetaDataImpl::icons() const
{
    return m_icons;
}

std::shared_ptr<IPermissionsImpl> OCIPackageMetaDataImpl::permissions() const
{
    return m_permissions;
}

std::optional<uint64_t> OCIPackageMetaDataImpl::storageQuota() const
{
    return m_storageQuota;
}

const std::optional<std::string> &OCIPackageMetaDataImpl::sharedStorageGroup() const
{
    return m_storageGroup;
}

std::optional<uint64_t> OCIPackageMetaDataImpl::memoryQuota() const
{
    return m_memoryQuota;
}

std::optional<uint64_t> OCIPackageMetaDataImpl::gpuMemoryQuota() const
{
    return m_gpuMemoryQuota;
}

LifecycleStates OCIPackageMetaDataImpl::supportedLifecycleStates() const
{
    return m_supportedLifecycleStates;
}

const std::vector<NetworkService> &OCIPackageMetaDataImpl::publicServices() const
{
    return m_publicServices;
}

const std::vector<NetworkService> &OCIPackageMetaDataImpl::exportedServices() const
{
    return m_exportedServices;
}

const std::vector<NetworkService> &OCIPackageMetaDataImpl::importedServices() const
{
    return m_importedServices;
}

const std::optional<DialInfo> &OCIPackageMetaDataImpl::dialInfo() const
{
    return m_dialInfo;
}

const std::optional<InputHandlingInfo> &OCIPackageMetaDataImpl::inputHandlingInfo() const
{
    return m_inputHandlingInfo;
}

const DisplayInfo &OCIPackageMetaDataImpl::displayInfo() const
{
    return m_displayInfo;
}

const AudioInfo &OCIPackageMetaDataImpl::audioInfo() const
{
    return m_audioInfo;
}

const std::optional<std::chrono::milliseconds> &OCIPackageMetaDataImpl::startTimeout() const
{
    return m_startTimeout;
}

const std::optional<std::chrono::milliseconds> &OCIPackageMetaDataImpl::watchdogInterval() const
{
    return m_watchdogInterval;
}

LoggingLevels OCIPackageMetaDataImpl::loggingLevels() const
{
    return m_loggingLevels;
}

JSON OCIPackageMetaDataImpl::vendorConfig(std::string_view key) const
{
    auto it = m_allConfigs.find(key);
    if (it != m_allConfigs.end())
        return it->second;

    return {};
}

std::set<std::string> OCIPackageMetaDataImpl::vendorConfigKeys() const
{
    std::set<std::string> keys;

    for (const auto &pair : m_allConfigs)
        keys.emplace(pair.first);

    return keys;
}

JSON OCIPackageMetaDataImpl::overrides(Override type) const
{
    auto it = m_overrides.find(type);
    if (it != m_overrides.end())
        return it->second;

    return {};
}
