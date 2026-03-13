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
#include "Result.h"
#include "core/Compatibility.h"
#include "core/IPackageMetaDataImpl.h"
#include "core/PermissionsImpl.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <vector>

class OCIPackageMetaDataImpl final : public LIBRALF_NS::IPackageMetaDataImpl
{
public:
    static LIBRALF_NS::Result<std::shared_ptr<OCIPackageMetaDataImpl>> fromConfigJson(const std::vector<uint8_t> &contents);

public:
    OCIPackageMetaDataImpl();
    ~OCIPackageMetaDataImpl() final = default;

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

private:
    using ProcessFieldFunction = std::function<bool(OCIPackageMetaDataImpl *_Nullable, const nlohmann::json &json)>;

    bool processId(const nlohmann::json &json);
    bool processVersion(const nlohmann::json &json);
    bool processVersionName(const nlohmann::json &json);
    bool processPackageType(const nlohmann::json &json);
    bool processPackageSpecifier(const nlohmann::json &json);
    bool processName(const nlohmann::json &json);
    bool processEntryPoint(const nlohmann::json &json);
    bool processEntryArgs(const nlohmann::json &json);
    bool processDependencies(const nlohmann::json &json);
    bool processPermissions(const nlohmann::json &json);
    bool processConfiguration(const nlohmann::json &json);

    bool processIconsConfig(const nlohmann::json &json);
    bool processStorageConfig(const nlohmann::json &json);
    bool processMemoryConfig(const nlohmann::json &json);
    bool processDialConfig(const nlohmann::json &json);
    bool processLogLevelsConfig(const nlohmann::json &json);
    bool processNetworkConfig(const nlohmann::json &json);
    bool processAppLifecycleConfig(const nlohmann::json &json);
    bool processLowPowerTerminateConfig(const nlohmann::json &json);
    bool processInputHandlingConfig(const nlohmann::json &json);
    bool processWindowConfig(const nlohmann::json &json);
    bool processDisplayConfig(const nlohmann::json &json);
    bool processAudioConfig(const nlohmann::json &json);
    bool processOverridesConfig(const nlohmann::json &json);

    static std::optional<uint64_t> parseMemorySize(const std::string &str);

private:
    std::string m_id;
    LIBRALF_NS::VersionNumber m_version;
    std::string m_versionName;
    std::string m_specifier;
    std::string m_mimeType;
    std::filesystem::path m_entryPointPath;
    std::list<std::string> m_entryArgs;
    std::map<std::string, LIBRALF_NS::VersionConstraint> m_dependencies;
    LIBRALF_NS::PackageType m_type = LIBRALF_NS::PackageType::Unknown;
    std::optional<std::string> m_title;
    LIBRALF_NS::PlatformInfo m_platformInfo;
    std::list<LIBRALF_NS::Icon> m_icons;
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
    std::map<std::string, LIBRALF_NS::JSON, std::less<>> m_allConfigs;
    std::map<LIBRALF_NS::Override, LIBRALF_NS::JSON> m_overrides;
};
