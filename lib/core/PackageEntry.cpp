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

#include "PackageEntry.h"
#include "IPackageEntryImpl.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

PackageEntry::PackageEntry() // NOLINT(modernize-use-equals-default)
{
}

PackageEntry::PackageEntry(PackageEntry &&other) noexcept
    : m_impl(std::move(other.m_impl))
{
}

PackageEntry::PackageEntry(std::unique_ptr<IPackageEntryImpl> &&impl)
    : m_impl(std::move(impl))
{
}

PackageEntry::~PackageEntry() // NOLINT(modernize-use-equals-default)
{
}

PackageEntry &PackageEntry::operator=(PackageEntry &&other) noexcept
{
    m_impl = std::move(other.m_impl);
    return *this;
}

const std::filesystem::path &PackageEntry::path() const
{
    static std::filesystem::path emptyPath = {};

    if (m_impl)
        return m_impl->path();
    else
        return emptyPath;
}

size_t PackageEntry::size() const
{
    if (m_impl)
        return m_impl->size();
    else
        return 0;
}

std::filesystem::perms PackageEntry::permissions() const
{
    if (m_impl)
        return m_impl->permissions();
    else
        return std::filesystem::perms::unknown;
}

time_t PackageEntry::modificationTime() const
{
    if (m_impl)
        return m_impl->modificationTime();
    else
        return 0;
}

uid_t PackageEntry::ownerId() const
{
    if (m_impl)
        return m_impl->ownerId();
    else
        return -1;
}

gid_t PackageEntry::groupId() const
{
    if (m_impl)
        return m_impl->groupId();
    else
        return -1;
}

std::filesystem::file_type PackageEntry::type() const
{
    if (m_impl)
        return m_impl->type();
    else
        return std::filesystem::file_type::not_found;
}

ssize_t PackageEntry::read(void *buf, size_t size, Error *error)
{
    if (m_impl)
    {
        return m_impl->read(buf, size, error);
    }
    else
    {
        if (error)
            error->assign(ErrorCode::InternalError, "No entry to read from");

        return -1;
    }
}
