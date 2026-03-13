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

#include "IPermissionsImpl.h"
#include "PackageMetaData.h"

namespace LIBRALF_NS
{

    class IPackageMetaDataImpl
    {
    public:
        virtual ~IPackageMetaDataImpl() = default;

        virtual const std::string &id() const = 0;

        virtual VersionNumber version() const = 0;

        virtual const std::string &versionName() const = 0;

        virtual PackageType type() const = 0;

        virtual const std::string &mimeType() const = 0;

        virtual const std::string &runtimeType() const = 0;

        virtual const std::optional<std::string> &title() const = 0;

        virtual const PlatformInfo &platformInfo() const = 0;

        virtual const std::filesystem::path &entryPointPath() const = 0;

        virtual const std::list<std::string> &entryArgs() const = 0;

        virtual const std::map<std::string, VersionConstraint> &dependencies() const = 0;

        virtual const std::list<Icon> &icons() const = 0;

        virtual std::optional<uint64_t> storageQuota() const = 0;

        virtual const std::optional<std::string> &sharedStorageGroup() const = 0;

        virtual std::optional<uint64_t> memoryQuota() const = 0;

        virtual std::optional<uint64_t> gpuMemoryQuota() const = 0;

        virtual std::shared_ptr<IPermissionsImpl> permissions() const = 0;

        virtual LifecycleStates supportedLifecycleStates() const = 0;

        virtual const std::vector<NetworkService> &publicServices() const = 0;

        virtual const std::vector<NetworkService> &exportedServices() const = 0;

        virtual const std::vector<NetworkService> &importedServices() const = 0;

        virtual const std::optional<DialInfo> &dialInfo() const = 0;

        virtual const std::optional<InputHandlingInfo> &inputHandlingInfo() const = 0;

        virtual const DisplayInfo &displayInfo() const = 0;

        virtual const AudioInfo &audioInfo() const = 0;

        virtual const std::optional<std::chrono::milliseconds> &startTimeout() const = 0;

        virtual const std::optional<std::chrono::milliseconds> &watchdogInterval() const = 0;

        virtual LoggingLevels loggingLevels() const = 0;

        virtual JSON vendorConfig(std::string_view key) const = 0;

        virtual std::set<std::string> vendorConfigKeys() const = 0;

        virtual JSON overrides(Override type) const = 0;
    };

} // namespace LIBRALF_NS
