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

#include "PackageAuxMetaData.h"
#include "IPackageAuxMetaDataImpl.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

PackageAuxMetaData::PackageAuxMetaData(std::unique_ptr<IPackageAuxMetaDataImpl> &&impl)
    : m_impl(std::move(impl))
{
}

PackageAuxMetaData::PackageAuxMetaData(PackageAuxMetaData &&other) noexcept // NOLINT(modernize-use-equals-default)
    : m_impl(std::move(other.m_impl))
{
}

PackageAuxMetaData::~PackageAuxMetaData() // NOLINT(modernize-use-equals-default)
{
}

PackageAuxMetaData &PackageAuxMetaData::operator=(PackageAuxMetaData &&other) noexcept // NOLINT(modernize-use-equals-default)
{
    m_impl = std::move(other.m_impl);
    return *this;
}

size_t PackageAuxMetaData::index() const
{
    if (!m_impl)
        return 0;
    else
        return m_impl->index();
}

std::string PackageAuxMetaData::mediaType() const
{
    if (!m_impl)
        return "";
    else
        return m_impl->mediaType();
}

std::map<std::string, std::string> PackageAuxMetaData::annotations() const
{
    if (!m_impl)
        return {};
    else
        return m_impl->annotations();
}

size_t PackageAuxMetaData::size() const
{
    if (!m_impl)
        return 0;
    else
        return m_impl->size();
}

Result<size_t> PackageAuxMetaData::read(void *buf, size_t size)
{
    if (!m_impl)
        return Error(ErrorCode::InvalidArgument, "PackageAuxMetaData is null");

    return m_impl->read(buf, size);
}

Result<> PackageAuxMetaData::seek(off_t position)
{
    if (!m_impl)
        return Error(ErrorCode::InvalidArgument, "PackageAuxMetaData is null");

    return m_impl->seek(position);
}

Result<std::vector<uint8_t>> PackageAuxMetaData::readAll()
{
    if (!m_impl)
        return Error(ErrorCode::InvalidArgument, "PackageAuxMetaData is null");

    return m_impl->readAll();
}
