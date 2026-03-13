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

#include <EntosTypes.h>
#include <PackageMetaData.h>

#include <map>
#include <sstream>
#include <string>
#include <string_view>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace std::literals::string_view_literals;

static const std::map<std::string_view, std::string_view, std::less<>> kConfigXmlPermissionsMap = {
    { INTERNET_PERMISSION ""sv, "wan-lan"sv },
    { HOME_APP_PERMISSION ""sv, "home-app"sv },
    { FIREBOLT_PERMISSION ""sv, "firebolt"sv },
    { THUNDER_PERMISSION ""sv, "thunder"sv },
    { RIALTO_PERMISSION ""sv, "rialto"sv },
    { GAME_CONTROLLER_PERMISSION ""sv, "game-controller"sv },
    { READ_EXTERNAL_STORAGE_PERMISSION ""sv, "read-external-storage"sv },
    { WRITE_EXTERNAL_STORAGE_PERMISSION ""sv, "write-external-storage"sv },
    { OVERLAY_PERMISSION ""sv, "issue-notifications"sv },
    { COMPOSITOR_PERMISSION ""sv, "compositor-app"sv },
    { TIME_SHIFT_BUFFER_PERMISSION ""sv, "tsb-storage"sv },

    { ENTOS_AS_ACCESS_LEVEL1_PERMISSION ""sv, "local-services-1"sv },
    { ENTOS_AS_ACCESS_LEVEL2_PERMISSION ""sv, "local-services-2"sv },
    { ENTOS_AS_ACCESS_LEVEL3_PERMISSION ""sv, "local-services-3"sv },
    { ENTOS_AS_ACCESS_LEVEL4_PERMISSION ""sv, "local-services-4"sv },
    { ENTOS_AS_ACCESS_LEVEL5_PERMISSION ""sv, "local-services-5"sv },
    { ENTOS_AS_POST_INTENT_PERMISSION ""sv, "post-intents"sv },
    { ENTOS_AS_PLAYER_PERMISSION ""sv, "as-player"sv },
    { ENTOS_AIRPLAY_PERMISSION ""sv, "airplay2"sv },
    { ENTOS_CHROMECAST_PERMISSION ""sv, "chromecast"sv },
    { ENTOS_BEARER_TOKEN_AUTHENTICATION_PERMISSION ""sv, "bearer-token-authentication"sv },
    { ENTOS_HTTPS_MTLS_AUTHENTICATION_PERMISSION ""sv, "https-mutual-authentication"sv },
    { ENTOS_ENTITLEMENT_INFO_PERMISSION ""sv, "stb-entitlements"sv },
    { ENTOS_TIME_SHIFT_BUFFER_PERMISSION ""sv, "tsb-storage"sv },
    { ENTOS_MEMORY_INTENSIVE_PERMISSION ""sv, "memory-intensive"sv },

};

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Helper to convert a vector, list or set of strings into a comma-separated
    string.
 */
template <typename Container>
static std::string setToString(const Container &set, const char delim = ',')
{
    std::ostringstream oss;
    for (const auto &item : set)
    {
        if (!oss.str().empty())
            oss << delim;
        oss << item;
    }
    return oss.str();
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Helper to convert JSON array into a comma-separated string.  Use this in
    preference to setToString() as that will add quotes around JSON string values.

 */
static std::string jsonArrayToString(const std::vector<JSON> &array, const char delim = ',')
{
    std::ostringstream oss;
    for (const auto &item : array)
    {
        if (!oss.str().empty())
            oss << delim;
        if (item.isString())
            oss << item.asString();
        else if (item.isInteger())
            oss << item.asInteger();
        else if (item.isBool())
            oss << (item.asBool() ? "true" : "false");
    }
    return oss.str();
}

static std::string_view toCapabilityValue(DisplaySize size)
{
    switch (size)
    {
        case DisplaySize::Size720x480:
            return "480"sv;
        case DisplaySize::Size720x576:
            return "576"sv;
        case DisplaySize::Size1280x720:
            return "720"sv;
        case DisplaySize::Size1920x1080:
            return "1080"sv;
        case DisplaySize::Size3840x2160:
            return "2160"sv;
        case DisplaySize::Size7680x4320:
            return "4320"sv;
        default:
            return {};
    }
}

static std::string toCapabilityValue(const std::vector<NetworkService> &services)
{
    std::ostringstream oss;
    for (const auto &service : services)
    {
        if (!oss.str().empty())
            oss << ',';
        if (service.protocol == NetworkService::Protocol::UDP)
            oss << "udp:";
        oss << service.port;
    }
    return oss.str();
}

static void appendMulticastCaps(std::map<std::string, std::string> *capabilities,
                                const std::map<std::string, JSON> &multicastConfig)
{
    if (multicastConfig.empty())
        return;

    for (const auto &[name, value] : multicastConfig)
    {
        if ((name == "serverSockets") && value.isArray())
        {
            std::ostringstream oss;
            const auto &sockets = value.asArray();
            for (const auto &socket : sockets)
            {
                if (socket.isObject())
                {
                    const auto &obj = socket.asObject();
                    oss << obj.at("name").asString() << ':' << obj.at("address").asString() << ':'
                        << obj.at("port").asInteger() << ",";
                }
            }

            std::string capValue = oss.str();
            if (!capValue.empty() && capValue.back() == ',')
                capValue.pop_back();

            capabilities->emplace("multicast-server-socket", capValue);
        }
        else if ((name == "clientSockets") && value.isArray())
        {
            std::ostringstream oss;
            const auto &sockets = value.asArray();
            for (const auto &socket : sockets)
            {
                if (socket.isObject())
                {
                    const auto &obj = socket.asObject();
                    oss << obj.at("name").asString() << ",";
                }
            }

            std::string capValue = oss.str();
            if (!capValue.empty() && capValue.back() == ',')
                capValue.pop_back();

            capabilities->emplace("multicast-client-socket", capValue);
        }
        else if (name == "forwarding")
        {
            // TODO:
        }
    }
}

static void appendFKPSCaps(std::map<std::string, std::string> *capabilities, const std::map<std::string, JSON> &fkpsConfig)
{
    auto it = fkpsConfig.find("files");
    if (it == fkpsConfig.end() || !it->second.isArray())
        return;

    const auto &fkpsFiles = it->second.asArray();
    if (fkpsFiles.empty())
        return;

    std::vector<std::string> files;
    files.reserve(fkpsFiles.size());
    for (const auto &file : fkpsFiles)
    {
        if (file.isString())
            files.emplace_back(file.asString());
    }

    if (!files.empty())
        capabilities->emplace("fkps", setToString(files, ','));
}

static void appendMediariteCaps(std::map<std::string, std::string> *capabilities,
                                const std::map<std::string, JSON> &mediariteConfig)
{
    for (const auto &[name, value] : mediariteConfig)
    {
        if ((name == "underlay") && value.isBool())
        {
            if (value.asBool())
                capabilities->emplace("mediarite-underlay", "");
        }
        else if ((name == "accessGroups") && value.isObject())
        {
            std::ostringstream ss;
            for (const auto &[groupName, groupAccess] : value.asObject())
            {
                if (!groupAccess.isArray())
                    continue;

                ss << groupName << ':' << jsonArrayToString(groupAccess.asArray(), ',') << ';';
            }

            std::string accessGroups = ss.str();
            if (!accessGroups.empty() && accessGroups.back() == ';')
                accessGroups.pop_back();

            capabilities->emplace("mapi", accessGroups);
        }
    }
}

static void appendLegacyDrmCaps(std::map<std::string, std::string> *capabilities,
                                const std::map<std::string, JSON> &legacyDrmConfig)
{
    for (const auto &[name, value] : legacyDrmConfig)
    {
        if ((name == "types") && value.isArray())
        {
            const auto &types = value.asArray();

            std::ostringstream ss;
            for (const auto &type : types)
            {
                if (type.isString())
                {
                    if (!ss.str().empty())
                        ss << ',';
                    ss << type.asString();
                }
            }
            capabilities->emplace("drm-type", ss.str());
        }
        else if ((name == "storageSizeKB") && value.isInteger())
        {
            const auto storageSize = value.asInteger();
            if (storageSize > 0)
            {
                capabilities->emplace("drm-store", std::to_string(storageSize));
            }
        }
    }
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Adds the capabilities from the given ApplicationAndServiceInfo object to the
    provided capabilities map.

    \see
 */
static void appendAppAndServiceCaps(std::map<std::string, std::string> *capabilities,
                                    const ApplicationAndServiceInfo *info)
{
    if (!info)
        return;

    // Process the permissions from the metadata
    std::optional<Permissions> permissions = info->permissions();
    if (permissions)
    {
        for (const auto &perm : kConfigXmlPermissionsMap)
        {
            if (permissions->get(perm.first))
                capabilities->emplace(perm.second, "");
        }
    }

    const auto storageQuota = info->storageQuota();
    if (storageQuota.has_value())
    {
        const auto mb = std::max<uint64_t>(1, storageQuota.value() / 1024 / 1024);
        capabilities->emplace("private-storage", std::to_string(mb));
    }

    const auto memoryQuota = info->memoryQuota();
    if (memoryQuota.has_value())
    {
        const auto mb = std::max<uint64_t>(1, memoryQuota.value() / 1024 / 1024);
        capabilities->emplace("sys-memory-limit", std::to_string(mb) + "m");
    }

    if ((info->supportedLifecycleStates() & LifecycleStates::Suspended) == LifecycleStates::Suspended)
        capabilities->emplace("suspend-mode", "");
    if ((info->supportedLifecycleStates() & LifecycleStates::Hibernated) == LifecycleStates::Hibernated)
        capabilities->emplace("hibernate-mode", "");
    if ((info->supportedLifecycleStates() & LifecycleStates::LowPower) != LifecycleStates::LowPower)
        capabilities->emplace("no-low-power-mode", "");

    const auto loggingLevels = info->loggingLevels();
    if (loggingLevels != LoggingLevels::Default)
    {
        std::ostringstream oss;
        if ((loggingLevels & LoggingLevels::Default) == LoggingLevels::Default)
            oss << "default,";
        if ((loggingLevels & LoggingLevels::Debug) == LoggingLevels::Debug)
            oss << "debug,";
        if ((loggingLevels & LoggingLevels::Info) == LoggingLevels::Info)
            oss << "info,";
        if ((loggingLevels & LoggingLevels::Milestone) == LoggingLevels::Milestone)
            oss << "milestone,";
        if ((loggingLevels & LoggingLevels::Error) == LoggingLevels::Error)
            oss << "error,";
        if ((loggingLevels & LoggingLevels::Warning) == LoggingLevels::Warning)
            oss << "warning,";
        if ((loggingLevels & LoggingLevels::Fatal) == LoggingLevels::Fatal)
            oss << "fatal,";

        std::string levels = oss.str();
        if (!levels.empty() && levels.back() == ',')
            levels.pop_back();

        capabilities->emplace("log-levels", levels);
    }

    if (!info->publicServices().empty())
        capabilities->emplace("hole-punch", toCapabilityValue(info->publicServices()));
    if (!info->exportedServices().empty())
        capabilities->emplace("local-socket-server", toCapabilityValue(info->exportedServices()));
    if (!info->importedServices().empty())
        capabilities->emplace("local-socket-client", toCapabilityValue(info->importedServices()));

    if (info->startTimeout())
        capabilities->emplace("start-timeout-sec", std::to_string(info->startTimeout()->count() / 1000));
    if (info->watchdogInterval())
        capabilities->emplace("watchdog-sec", std::to_string(info->watchdogInterval()->count() / 1000));

    if (info->sharedStorageGroup())
        capabilities->emplace("child-app", info->sharedStorageGroup().value());
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Adds the capabilities from the given ApplicationInfo object to the provided
    capabilities map.

*/
static void appendAppCaps(std::map<std::string, std::string> *capabilities, const ApplicationInfo *appInfo)
{
    const auto &dialInfo = appInfo->dialInfo();
    if (dialInfo)
    {
        capabilities->emplace("dial-app", setToString(dialInfo->corsDomains));

        if (!dialInfo->dialIds.empty())
            capabilities->emplace("dial-id", setToString(dialInfo->dialIds));

        if (dialInfo->originHeaderRequired)
            capabilities->emplace("dial-origin-mandatory", "");
    }

    const auto &inputHandling = appInfo->inputHandlingInfo();
    if (inputHandling)
    {
        if (!inputHandling->capturedKeys.empty())
            capabilities->emplace("keymapping", setToString(inputHandling->capturedKeys));
        if (!inputHandling->monitoredKeys.empty())
            capabilities->emplace("forward-keymapping", setToString(inputHandling->monitoredKeys));
    }

    const auto &displayInfo = appInfo->displayInfo();
    if (displayInfo.size != DisplaySize::Default)
        capabilities->emplace("virtual-resolution", toCapabilityValue(displayInfo.size));
    if (displayInfo.refreshRate == DisplayRefreshRate::SixtyHz)
        capabilities->emplace("refresh-rate-60hz", "");
    if (displayInfo.pictureMode == "gameModeStatic")
        capabilities->emplace("game-mode", "static");
    else if (displayInfo.pictureMode == "gameModeDynamic")
        capabilities->emplace("game-mode", "dynamic");

    const auto gpuMemoryQuota = appInfo->gpuMemoryQuota();
    if (gpuMemoryQuota.has_value())
    {
        const auto mb = std::max<uint64_t>(1, gpuMemoryQuota.value() / 1024 / 1024);
        capabilities->emplace("gpu-memory-limit", std::to_string(mb) + "m");
    }

    const auto &audioInfo = appInfo->audioInfo();
    if (audioInfo.soundMode)
        capabilities->emplace("sound-mode", audioInfo.soundMode.value());
    if (audioInfo.soundScene)
        capabilities->emplace("sound-scene", audioInfo.soundScene.value());
    if (audioInfo.loudnessAdjustment)
        capabilities->emplace("program-reference-level", std::to_string(audioInfo.loudnessAdjustment.value()));

    // Append the metadata common to both apps and services
    appendAppAndServiceCaps(capabilities, appInfo);
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Adds the capabilities from the given ServiceInfo object to the provided
    capabilities map.

 */
static void appendServiceCaps(std::map<std::string, std::string> *capabilities, const ServiceInfo *serviceInfo)
{
    capabilities->emplace("daemon-app", "");
    capabilities->emplace("system-app", "");

    appendAppAndServiceCaps(capabilities, serviceInfo);
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Adds the capabilities from the given marketplace intercept config to the
    provided capabilities map.
 */
static void appendMarketplaceInterceptCaps(std::map<std::string, std::string> *capabilities,
                                           const std::map<std::string, JSON> &marketplaceInterceptConfig)
{
    for (const auto &[name, value] : marketplaceInterceptConfig)
    {
        if ((name == "enable") && value.isBool())
        {
            capabilities->emplace("intercept", value.asBool() ? "true" : "false");
        }
    }
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Generates a map of all the capabilities for the legacy config.xml format.
    This is the inverse of the `W3CPackageMetaDataBuilder` code that generates
    the meta-data object from the config.xml format.

    The medium term issue with this is that the config.xml format is constantly
    being extended with new capabilities, and every time that happens this code
    needs to be updated to include the new capabilities.  This is not ideal, but
    hopefully this code is just a stop-gap until all packages are in the new
    format.

    \see
 */
static std::map<std::string, std::string> genConfigXmlCapabilities(const PackageMetaData &metadata)
{
    // A runtime or base package will not have any capabilities, so return an empty map.
    if ((metadata.type() == PackageType::Runtime) || (metadata.type() == PackageType::Base))
        return {};

    std::map<std::string, std::string> capabilities;

    // Process app and service specific capabilities
    if (metadata.type() == PackageType::Application)
    {
        auto appInfo = metadata.applicationInfo();
        if (!appInfo)
            return {};

        appendAppCaps(&capabilities, &(appInfo.value()));
    }
    else if (metadata.type() == PackageType::Service)
    {
        auto serviceInfo = metadata.serviceInfo();
        if (!serviceInfo)
            return {};

        appendServiceCaps(&capabilities, &(serviceInfo.value()));
    }

    // Most vendor configs are just strings that can be directly mapped into the capabilities, so this table is the
    // list of vendor config keys to capability names.
    const std::map<std::string_view, std::string_view, std::less<>> kVendorConfigToCapabilityMap = {
        { ENTOS_CATALOGUE_ID_CONFIGURATION, "catalogue-id"sv },
        { ENTOS_CONTENT_PARTNER_ID_CONFIGURATION, "content-partner-id"sv },
        { ENTOS_PIN_MANAGEMENT_CONFIGURATION, "pin-management"sv },
        { ENTOS_AGE_POLICY_CONFIGURATION, "age-policy"sv },
        { ENTOS_AGE_RATING_CONFIGURATION, "age-rating"sv },
        { ENTOS_PRELAUNCH_CONFIGURATION, "pre-launch"sv },
    };

    for (const auto &[key, capability] : kVendorConfigToCapabilityMap)
    {
        const auto value = metadata.vendorConfig(key);
        if (value.isString())
            capabilities.emplace(capability.data(), value.asString());
        else if (value.isInteger())
            capabilities.emplace(capability.data(), std::to_string(value.asInteger()));
        else if (!value.isNull())
            fprintf(stderr, "Warning: vendor config key '%s' is not a string or integer, skipping.\n", key.data());
    }

    // Serialise the mediarite configuration
    const auto mapiConfig = metadata.vendorConfig(ENTOS_MEDIARITE_CONFIGURATION);
    if (mapiConfig.isObject())
        appendMediariteCaps(&capabilities, mapiConfig.asObject());

    // Serialise the multicast configuration
    const auto multicastConfig = metadata.vendorConfig(ENTOS_MULTICAST_CONFIGURATION);
    if (multicastConfig.isObject())
        appendMulticastCaps(&capabilities, multicastConfig.asObject());

    // Serialise the FKPS configuration
    const auto fkpsConfig = metadata.vendorConfig(ENTOS_FKPS_CONFIGURATION);
    if (fkpsConfig.isObject())
        appendFKPSCaps(&capabilities, fkpsConfig.asObject());

    // Serialise the legacy drm configuration
    const auto drmConfig = metadata.vendorConfig(ENTOS_LEGACY_DRM_CONFIGURATION);
    if (drmConfig.isObject())
        appendLegacyDrmCaps(&capabilities, drmConfig.asObject());

    // Serialise the marketplace intercept configuration
    const auto marketplaceInterceptConfig = metadata.vendorConfig(ENTOS_MARKETPLACE_INTERCEPT_CONFIGURATION);
    if (marketplaceInterceptConfig.isObject())
        appendMarketplaceInterceptCaps(&capabilities, marketplaceInterceptConfig.asObject());

    return capabilities;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Generates a map of platform filters for the legacy config.xml format.

 */
static std::list<std::pair<std::string, std::string>> genConfigXmlPlatformFilters(const PackageMetaData &metadata)
{
    const auto config = metadata.vendorConfig(ENTOS_PLATFORM_FILTERS_CONFIGURATION);
    if (!config.isObject())
    {
        fprintf(stderr, "Warning: vendor config key '%s' is not an object, skipping platform filters.\n",
                ENTOS_PLATFORM_FILTERS_CONFIGURATION);
        return {};
    }

    // For some reason the standard schema requires the various platform filters to be in order, even though the
    // actual code ignores the order.  So we will add them in the order they are defined in the schema ... which makes
    // this function a bit more complex than it needs to be.
    std::set<std::string> platformIds;
    std::set<std::string> platformVariants;
    std::set<std::string> propositions;
    std::set<std::string> countries;
    std::set<std::string> subdivisions;
    std::set<std::string> yoctoVersions;

    const auto &filters = config.asObject();
    for (const auto &[key, value] : filters)
    {
        if (!value.isArray())
        {
            fprintf(stderr, "Warning: vendor config key '%s' is not an array, skipping platform filter '%s'.\n",
                    ENTOS_PLATFORM_FILTERS_CONFIGURATION, key.c_str());
            continue;
        }

        const auto &filterValues = value.asArray();
        for (const auto &filterValue : filterValues)
        {
            if (!filterValue.isString())
                continue;

            const auto &strValue = filterValue.asString();

            if (key == "platformIds")
                platformIds.emplace(strValue);
            if (key == "platformVariants")
                platformVariants.emplace(strValue);
            else if (key == "propositions")
                propositions.emplace(strValue);
            else if (key == "countries")
                countries.emplace(strValue);
            else if (key == "subdivisions")
                subdivisions.emplace(strValue);
            else if (key == "yoctoVersions")
                yoctoVersions.emplace(strValue);
        }
    }

    std::list<std::pair<std::string, std::string>> platformFilters;
    for (const auto &value : platformIds)
        platformFilters.emplace_back("platformId", value);
    for (const auto &value : platformVariants)
        platformFilters.emplace_back("platformVariant", value);
    for (const auto &value : propositions)
        platformFilters.emplace_back("proposition", value);
    for (const auto &value : countries)
        platformFilters.emplace_back("country", value);
    for (const auto &value : subdivisions)
        platformFilters.emplace_back("subdivision", value);
    for (const auto &value : yoctoVersions)
        platformFilters.emplace_back("yoctoVersion", value);

    return platformFilters;
}

// -----------------------------------------------------------------------------
/*!
    Helper to escape XML special characters in a string.

*/
std::string xmlEscape(std::string str)
{
    static const std::vector<std::pair<char, std::string_view>> escapeChars = {
        { '&', "&amp;"sv }, { '<', "&lt;"sv }, { '>', "&gt;"sv }, { '"', "&quot;"sv }, { '\'', "&apos;"sv }
    };

    for (const auto &[ch, escape] : escapeChars)
    {
        size_t pos = 0;
        while ((pos = str.find(ch, pos)) != std::string::npos)
        {
            str.replace(pos, 1, escape);
            pos += escape.size();
        }
    }

    return str;
}

// -----------------------------------------------------------------------------
/*!
    Helper function that converts the package meta-data to the legacy config.xml
    format.

    This is only used for backwards compatibility tooling and is should
    effectively do the opposite of the `W3CPackageMetaDataBuilder` code.

*/
std::string metaDataToConfigXml(const PackageMetaData &metadata)
{
    std::ostringstream stream;

    stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<widget xmlns=\"http://www.bskyb.com/ns/widgets\" version=\"2.0\">\n";

    stream << "    <name short=\"" << metadata.id() << "\" version=\"" << xmlEscape(metadata.versionName()) << "\">"
           << xmlEscape(metadata.title().value_or("")) << "</name>\n";

    const auto &icons = metadata.icons();
    if (!icons.empty())
    {
        const auto &icon = icons.front();
        stream << "    <icon src=\"" << icon.path.string() << "\" type=\"" << icon.mimeType << "\"/>\n";
    }

    // set the content src and type
    auto contentType = metadata.mimeType();
    auto idx = contentType.find("service/"sv);
    if (idx != std::string::npos)
        contentType.replace(idx, 8, "application/"sv);

    stream << "    <content src=\"" << metadata.entryPointPath().string() << "\" type=\"" << contentType << "\">\n";

    // set any platform filters
    stream << "        <platformFilters>\n";
    const auto platformFilters = genConfigXmlPlatformFilters(metadata);
    for (const auto &filter : platformFilters)
    {
        stream << "            <" << filter.first << " name=\"" << filter.second << "\"/>\n";
    }
    stream << "        </platformFilters>\n"
           << "    </content>\n";

    // build a map of capabilities and then write them to the xml
    stream << "    <capabilities>\n";
    const auto capabilities = genConfigXmlCapabilities(metadata);
    for (const auto &capability : capabilities)
    {
        stream << "        <capability name=\"" << capability.first << "\"";
        if (!capability.second.empty())
            stream << ">" << capability.second << "</capability>\n";
        else
            stream << "/>\n";
    }
    stream << "    </capabilities>\n";

    // for backwards compatibility check the deprecated parental control setting
    const auto parentalControl = metadata.vendorConfig(ENTOS_PARENTAL_CONTROL_CONFIGURATION);
    if (parentalControl.isBool())
    {
        stream << "    <parentalControl>" << (parentalControl.asBool() ? "true" : "false") << "</parentalControl>\n";
    }

    stream << "</widget>\n";
    return stream.str();
}
