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
#include "core/Compatibility.h"
#include "core/IPackageMetaDataImpl.h"
#include "core/PackageAuxMetaDataImpl.h"
#include "core/PermissionsImpl.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <functional>
#include <map>
#include <vector>

using xmlDocUniquePtr = std::unique_ptr<xmlDoc, std::function<void(xmlDoc *_Nonnull)>>;

class W3CPackageMetaDataImpl final : public LIBRALF_NS::IPackageMetaDataImpl
{
public:
    static std::shared_ptr<W3CPackageMetaDataImpl> fromConfigXml(const std::vector<uint8_t> &contents,
                                                                 LIBRALF_NS::Error *_Nullable error);

public:
    W3CPackageMetaDataImpl();
    ~W3CPackageMetaDataImpl() final = default;

    const std::string &id() const final;

    LIBRALF_NS::VersionNumber version() const final;

    const std::string &versionName() const final;

    LIBRALF_NS::PackageType type() const final;

    const std::string &mimeType() const final;

    const std::string &runtimeType() const final;

    const std::optional<std::string> &title() const final;

    const LIBRALF_NS::PlatformInfo &platformInfo() const final;

    const std::filesystem::path &entryPointPath() const final;

    const std::list<std::string> &entryArgs() const final;

    const std::map<std::string, LIBRALF_NS::VersionConstraint> &dependencies() const final;

    const std::list<LIBRALF_NS::Icon> &icons() const final;

    std::shared_ptr<LIBRALF_NS::IPermissionsImpl> permissions() const final;

    std::optional<uint64_t> storageQuota() const final;

    const std::optional<std::string> &sharedStorageGroup() const final;

    std::optional<uint64_t> memoryQuota() const final;

    std::optional<uint64_t> gpuMemoryQuota() const final;

    LIBRALF_NS::LifecycleStates supportedLifecycleStates() const final;

    const std::vector<LIBRALF_NS::NetworkService> &publicServices() const final;

    const std::vector<LIBRALF_NS::NetworkService> &exportedServices() const final;

    const std::vector<LIBRALF_NS::NetworkService> &importedServices() const final;

    const std::optional<LIBRALF_NS::DialInfo> &dialInfo() const final;

    const std::optional<LIBRALF_NS::InputHandlingInfo> &inputHandlingInfo() const final;

    const LIBRALF_NS::DisplayInfo &displayInfo() const final;

    const LIBRALF_NS::AudioInfo &audioInfo() const final;

    const std::optional<std::chrono::milliseconds> &startTimeout() const final;

    const std::optional<std::chrono::milliseconds> &watchdogInterval() const final;

    LIBRALF_NS::LoggingLevels loggingLevels() const final;

    LIBRALF_NS::JSON vendorConfig(std::string_view key) const final;

    std::set<std::string> vendorConfigKeys() const final;

    LIBRALF_NS::JSON overrides(LIBRALF_NS::Override type) const final;

    bool hasAuxMetaDataFile(std::string_view mediaType) const;

    bool addAuxMetaDataFile(std::string_view mediaType, std::vector<uint8_t> data);

    LIBRALF_NS::Result<std::unique_ptr<LIBRALF_NS::PackageAuxMetaDataImpl>>
    getAuxMetaDataFile(std::string_view mediaType) const;

    std::set<std::string> availAuxMetaData() const;

private:
    static bool processName(W3CPackageMetaDataImpl *_Nonnull metaData, const xmlNode *_Nonnull nameElement,
                            LIBRALF_NS::Error *_Nullable error);

    static bool processIcon(W3CPackageMetaDataImpl *_Nonnull metaData, const xmlNode *_Nonnull iconElement,
                            LIBRALF_NS::Error *_Nullable error);

    static bool processContentV1(W3CPackageMetaDataImpl *_Nonnull metaData, const xmlNode *_Nonnull contentElement,
                                 LIBRALF_NS::Error *_Nullable error);

    static bool processContentV2(W3CPackageMetaDataImpl *_Nonnull metaData, const xmlNode *_Nonnull contentElement,
                                 LIBRALF_NS::Error *_Nullable error);

    static bool processCapabilities(W3CPackageMetaDataImpl *_Nonnull metaData,
                                    const xmlNode *_Nonnull capabilitiesElement, LIBRALF_NS::Error *_Nullable error);

    static bool processCapability(W3CPackageMetaDataImpl *_Nonnull metaData, const std::string &name,
                                  const std::string &content, LIBRALF_NS::Error *_Nullable error);

    static bool processParentalControl(W3CPackageMetaDataImpl *_Nonnull metaData,
                                       const xmlNode *_Nonnull parentalControlElement,
                                       LIBRALF_NS::Error *_Nullable error);

#define CAPABILITY_HANDLER(name)                                                                                       \
    static bool processCapability##name(W3CPackageMetaDataImpl *_Nonnull metaData, const std::string &(name),          \
                                        const std::string &content, LIBRALF_NS::Error *_Nullable error)

    CAPABILITY_HANDLER(StorageSize);
    CAPABILITY_HANDLER(LifecycleStates);
    CAPABILITY_HANDLER(Services);
    CAPABILITY_HANDLER(ParentPackageId);
    CAPABILITY_HANDLER(DialInfo);
    CAPABILITY_HANDLER(KeyMapping);
    CAPABILITY_HANDLER(DisplayInfo);
    CAPABILITY_HANDLER(AudioInfo);
    CAPABILITY_HANDLER(SystemApp);
    CAPABILITY_HANDLER(FKPS);
    CAPABILITY_HANDLER(Mediarite);
    CAPABILITY_HANDLER(PreLaunch);
    CAPABILITY_HANDLER(AgeRating);
    CAPABILITY_HANDLER(AgePolicy);
    CAPABILITY_HANDLER(CatalogueId);
    CAPABILITY_HANDLER(ContentPartnerId);
    CAPABILITY_HANDLER(Logging);
    CAPABILITY_HANDLER(Watchdog);
    CAPABILITY_HANDLER(MemoryLimits);
    CAPABILITY_HANDLER(Drm);
    CAPABILITY_HANDLER(PinManagement);
    CAPABILITY_HANDLER(Multicast);
    CAPABILITY_HANDLER(AppIntercept);

#undef CAPABILITY_HANDLER

    static std::string &trimString(std::string &str);
    static std::set<std::string> splitStringToSet(const std::string &str, char delim = ',');
    static std::vector<std::string> splitStringToVector(const std::string &str, char delim = ',');
    static LIBRALF_NS::JSON splitStringToJsonArray(const std::string &str, char delim = ',');

private:
    std::string m_id;
    std::string m_version;
    std::string m_contentType;
    std::string m_runtimeType;
    std::filesystem::path m_entryPointPath;
    LIBRALF_NS::PackageType m_type = LIBRALF_NS::PackageType::Unknown;
    std::optional<std::string> m_title;
    LIBRALF_NS::PlatformInfo m_platformInfo;
    std::list<LIBRALF_NS::Icon> m_icons;
    std::optional<std::filesystem::path> m_iconPath;
    std::optional<std::string> m_iconMimeType;
    std::optional<uint64_t> m_storageQuota;
    std::optional<std::string> m_storageGroup;
    std::optional<uint64_t> m_memoryQuota;
    std::optional<uint64_t> m_gpuMemoryQuota;

    std::shared_ptr<PermissionsImpl> m_permissions;
    LIBRALF_NS::LifecycleStates m_supportedLifecycleStates =
        (LIBRALF_NS::LifecycleStates::Paused | LIBRALF_NS::LifecycleStates::LowPower);
    std::vector<LIBRALF_NS::NetworkService> m_publicServices;
    std::vector<LIBRALF_NS::NetworkService> m_exportedServices;
    std::vector<LIBRALF_NS::NetworkService> m_importedServices;
    std::optional<LIBRALF_NS::DialInfo> m_dialInfo;
    std::optional<LIBRALF_NS::InputHandlingInfo> m_inputHandlingInfo;
    LIBRALF_NS::DisplayInfo m_displayInfo;
    LIBRALF_NS::AudioInfo m_audioInfo;
    LIBRALF_NS::LoggingLevels m_loggingLevels = LIBRALF_NS::LoggingLevel::Default;
    std::optional<std::chrono::milliseconds> m_startTimeout;
    std::optional<std::chrono::milliseconds> m_watchdogInterval;

    std::map<std::string, LIBRALF_NS::JSON, std::less<>> m_vendorConfig;
    std::map<std::string, std::vector<uint8_t>, std::less<>> m_auxMetaDataFiles;
};
