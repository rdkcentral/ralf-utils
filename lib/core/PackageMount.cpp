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

#include "PackageMount.h"
#include "IPackageMountImpl.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

PackageMount::PackageMount(std::unique_ptr<IPackageMountImpl> &&impl)
    : m_impl(std::move(impl))
{
}

PackageMount::PackageMount(PackageMount &&other) noexcept
    : m_impl(std::move(other.m_impl))
{
}

PackageMount::~PackageMount() // NOLINT(modernize-use-equals-default)
{
}

PackageMount &PackageMount::operator=(PackageMount &&other) noexcept
{
    m_impl = std::move(other.m_impl);
    return *this;
}

bool PackageMount::isMounted() const
{
    if (m_impl)
        return m_impl->isMounted();
    else
        return false;
}

std::filesystem::path PackageMount::mountPoint() const
{
    if (m_impl)
        return m_impl->mountPoint();
    else
        return {};
}

void PackageMount::unmount()
{
    if (m_impl)
        m_impl->unmount();
}

void PackageMount::detach()
{
    if (m_impl)
        m_impl->detach();
}

std::string PackageMount::volumeName() const
{
    if (m_impl)
        return m_impl->volumeName();
    else
        return {};
}

std::string PackageMount::volumeUuid() const
{
    if (m_impl)
        return m_impl->volumeUuid();
    else
        return {};
}

MountStatus PackageMount::status() const
{
    if (m_impl)
        return m_impl->status();
    else
        return MountStatus::NotMounted;
}
