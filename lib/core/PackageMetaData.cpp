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

#include "PackageMetaData.h"
#include "IPackageMetaDataImpl.h"
#include "PermissionsImpl.h"

#include <sstream>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

static const std::string kEmptyString;
static const std::filesystem::path kEmptyPath;

template <typename T>
static const std::optional<T> &kEmptyOptional()
{
    static const std::optional<T> kEmpty;
    return kEmpty;
}

PackageMetaData::PackageMetaData() // NOLINT(modernize-use-equals-default)
{
}

PackageMetaData::PackageMetaData(std::shared_ptr<IPackageMetaDataImpl> impl)
    : m_impl(std::move(impl))
{
}

PackageMetaData::PackageMetaData(const PackageMetaData &other) // NOLINT(modernize-use-equals-default)
    : m_impl(other.m_impl)
{
}

PackageMetaData::PackageMetaData(PackageMetaData &&other) noexcept // NOLINT(modernize-use-equals-default)
    : m_impl(std::move(other.m_impl))
{
}

PackageMetaData::~PackageMetaData() // NOLINT(modernize-use-equals-default)
{
}

PackageMetaData &PackageMetaData::operator=(PackageMetaData &&other) noexcept // NOLINT(modernize-use-equals-default)
{
    m_impl = std::move(other.m_impl);
    return *this;
}

PackageMetaData &PackageMetaData::operator=(const PackageMetaData &other) // NOLINT(modernize-use-equals-default)
{
    if (this != &other)
        m_impl = other.m_impl;

    return *this;
}
bool PackageMetaData::isValid() const
{
    return m_impl != nullptr;
}

bool PackageMetaData::isNull() const
{
    return m_impl == nullptr;
}

const std::string &PackageMetaData::id() const
{
    if (!m_impl)
        return kEmptyString;
    else
        return m_impl->id();
}

VersionNumber PackageMetaData::version() const
{
    if (!m_impl)
        return {};
    else
        return m_impl->version();
}

const std::string &PackageMetaData::versionName() const
{
    if (!m_impl)
        return kEmptyString;
    else
        return m_impl->versionName();
}

PackageType PackageMetaData::type() const
{
    if (!m_impl)
        return PackageType::Unknown;
    else
        return m_impl->type();
}

const std::string &PackageMetaData::mimeType() const
{
    if (!m_impl)
        return kEmptyString;
    else
        return m_impl->mimeType();
}

const std::optional<std::string> &PackageMetaData::title() const
{
    if (!m_impl)
        return kEmptyOptional<std::string>();
    else
        return m_impl->title();
}

const std::filesystem::path &PackageMetaData::entryPointPath() const
{
    if (!m_impl)
        return kEmptyPath;
    else
        return m_impl->entryPointPath();
}

const std::list<std::string> &PackageMetaData::entryArgs() const
{
    static const std::list<std::string> kEmptyEntryArgs;

    if (!m_impl)
        return kEmptyEntryArgs;
    else
        return m_impl->entryArgs();
}

const std::map<std::string, VersionConstraint> &PackageMetaData::dependencies() const
{
    static const std::map<std::string, VersionConstraint> kEmptyDependencies;

    if (!m_impl)
        return kEmptyDependencies;
    else
        return m_impl->dependencies();
}

const std::list<Icon> &PackageMetaData::icons() const
{
    static const std::list<Icon> kEmptyIcons;

    if (!m_impl)
        return kEmptyIcons;
    else
        return m_impl->icons();
}

const PlatformInfo &PackageMetaData::platformInfo() const
{
    static const PlatformInfo kEmptyPlatformInfo;

    if (!m_impl)
        return kEmptyPlatformInfo;
    else
        return m_impl->platformInfo();
}

std::optional<ApplicationInfo> PackageMetaData::applicationInfo() const
{
    if (!m_impl || (m_impl->type() != PackageType::Application))
        return std::nullopt;
    else
        return ApplicationInfo(m_impl);
}

std::optional<ServiceInfo> PackageMetaData::serviceInfo() const
{
    if (!m_impl || (m_impl->type() != PackageType::Service))
        return std::nullopt;
    else
        return ServiceInfo(m_impl);
}

std::optional<RuntimeInfo> PackageMetaData::runtimeInfo() const
{
    if (!m_impl || (m_impl->type() != PackageType::Runtime))
        return std::nullopt;
    else
        return RuntimeInfo(m_impl);
}

JSON PackageMetaData::vendorConfig(std::string_view key) const
{
    if (!m_impl)
        return {};
    else
        return m_impl->vendorConfig(key);
}

std::set<std::string> PackageMetaData::vendorConfigKeys() const
{
    if (!m_impl)
        return {};
    else
        return m_impl->vendorConfigKeys();
}

JSON PackageMetaData::overrides(Override type) const
{
    if (!m_impl)
        return {};
    else
        return m_impl->overrides(type);
}

ApplicationInfo::ApplicationInfo(std::shared_ptr<IPackageMetaDataImpl> impl)
    : ApplicationAndServiceInfo(std::move(impl))
{
}

const std::optional<DialInfo> &ApplicationInfo::dialInfo() const
{
    if (!m_impl)
        return kEmptyOptional<DialInfo>();
    else
        return m_impl->dialInfo();
}

const std::optional<InputHandlingInfo> &ApplicationInfo::inputHandlingInfo() const
{
    if (!m_impl)
        return kEmptyOptional<InputHandlingInfo>();
    else
        return m_impl->inputHandlingInfo();
}

const DisplayInfo &ApplicationInfo::displayInfo() const
{
    static const DisplayInfo kEmptyDisplayInfo;
    if (!m_impl)
        return kEmptyDisplayInfo;
    else
        return m_impl->displayInfo();
}

const AudioInfo &ApplicationInfo::audioInfo() const
{
    static const AudioInfo kEmptyAudioInfo;
    if (!m_impl)
        return kEmptyAudioInfo;
    else
        return m_impl->audioInfo();
}

std::optional<uint64_t> ApplicationInfo::gpuMemoryQuota() const
{
    if (!m_impl)
        return kEmptyOptional<uint64_t>();
    else
        return m_impl->gpuMemoryQuota();
}

ServiceInfo::ServiceInfo(std::shared_ptr<IPackageMetaDataImpl> impl)
    : ApplicationAndServiceInfo(std::move(impl))
{
}

ApplicationAndServiceInfo::ApplicationAndServiceInfo(std::shared_ptr<IPackageMetaDataImpl> impl)
    : m_impl(std::move(impl))
{
}

const std::string &ApplicationAndServiceInfo::runtimeType() const
{
    if (!m_impl)
        return kEmptyString;
    else
        return m_impl->runtimeType();
}

Permissions ApplicationAndServiceInfo::permissions() const
{
    if (!m_impl)
        return {};
    else
        return Permissions(m_impl->permissions());
}

std::optional<uint64_t> ApplicationAndServiceInfo::storageQuota() const
{
    if (!m_impl)
        return kEmptyOptional<uint64_t>();
    else
        return m_impl->storageQuota();
}

const std::optional<std::string> &ApplicationAndServiceInfo::sharedStorageGroup() const
{
    if (!m_impl)
        return kEmptyOptional<std::string>();
    else
        return m_impl->sharedStorageGroup();
}

std::optional<uint64_t> ApplicationAndServiceInfo::memoryQuota() const
{
    if (!m_impl)
        return kEmptyOptional<uint64_t>();
    else
        return m_impl->memoryQuota();
}

LifecycleStates ApplicationAndServiceInfo::supportedLifecycleStates() const
{
    if (!m_impl)
        return static_cast<LifecycleStates>(0);
    else
        return m_impl->supportedLifecycleStates();
}

const std::vector<NetworkService> &ApplicationAndServiceInfo::publicServices() const
{
    static const std::vector<NetworkService> kEmptyServices;
    if (!m_impl)
        return kEmptyServices;
    else
        return m_impl->publicServices();
}

const std::vector<NetworkService> &ApplicationAndServiceInfo::exportedServices() const
{
    static const std::vector<NetworkService> kEmptyServices;
    if (!m_impl)
        return kEmptyServices;
    else
        return m_impl->exportedServices();
}

const std::vector<NetworkService> &ApplicationAndServiceInfo::importedServices() const
{
    static const std::vector<NetworkService> kEmptyServices;
    if (!m_impl)
        return kEmptyServices;
    else
        return m_impl->importedServices();
}

const std::optional<std::chrono::milliseconds> &ApplicationAndServiceInfo::startTimeout() const
{
    if (!m_impl)
        return kEmptyOptional<std::chrono::milliseconds>();
    else
        return m_impl->startTimeout();
}

const std::optional<std::chrono::milliseconds> &ApplicationAndServiceInfo::watchdogInterval() const
{
    if (!m_impl)
        return kEmptyOptional<std::chrono::milliseconds>();
    else
        return m_impl->watchdogInterval();
}

LoggingLevels ApplicationAndServiceInfo::loggingLevels() const
{
    if (!m_impl)
        return LoggingLevel::Default;
    else
        return m_impl->loggingLevels();
}

RuntimeInfo::RuntimeInfo(std::shared_ptr<IPackageMetaDataImpl> impl)
    : m_impl(std::move(impl))
{
}

// NOLINTBEGIN(misc-no-recursion): Allow recursive JSON types

std::ostream &LIBRALF_NS::operator<<(std::ostream &s, const JSON &j)
{
    if (j.isNull())
    {
        s << "null";
    }
    else if (j.isBool())
    {
        s << (j.asBool() ? "true" : "false");
    }
    else if (j.isInteger())
    {
        s << j.asInteger();
    }
    else if (j.isDouble())
    {
        s << j.asDouble();
    }
    else if (j.isString())
    {
        s << '\"' << j.asString() << '\"';
    }
    else if (j.isArray())
    {
        s << "[ ";
        const auto &array = j.asArray();
        for (auto it = array.begin(); it != array.end(); ++it)
        {
            if (it != array.begin())
                s << ", ";
            s << *it;
        }
        s << " ]";
    }
    else if (j.isObject())
    {
        s << "{ ";
        const auto &obj = j.asObject();
        for (auto it = obj.begin(); it != obj.end(); ++it)
        {
            if (it != obj.begin())
                s << ", ";
            s << '\"' << it->first << "\": " << it->second;
        }
        s << " }";
    }

    return s;
}

// NOLINTEND(misc-no-recursion)
