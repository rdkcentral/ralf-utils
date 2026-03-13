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

#include "PackageAuxMetaDataImpl.h"

#include <cstring>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

PackageAuxMetaDataImpl::PackageAuxMetaDataImpl(std::string_view mediaType, size_t index, std::vector<uint8_t> data,
                                               std::map<std::string, std::string> annotations)
    : m_mediaType(mediaType)
    , m_index(index)
    , m_data(std::move(data))
    , m_annotations(std::move(annotations))
    , m_currentOffset(0)
{
}

size_t PackageAuxMetaDataImpl::index() const
{
    return m_index;
}

std::string PackageAuxMetaDataImpl::mediaType() const
{
    return m_mediaType;
}

std::map<std::string, std::string> PackageAuxMetaDataImpl::annotations() const
{
    return m_annotations;
}

size_t PackageAuxMetaDataImpl::size() const
{
    return m_data.size();
}

Result<size_t> PackageAuxMetaDataImpl::read(void *buf, size_t size)
{
    if (!buf)
        return Error(ErrorCode::InvalidArgument, "Buffer cannot be null");
    if (m_currentOffset >= m_data.size())
        return 0;

    size = std::min(size, m_data.size() - m_currentOffset);
    if (size > 0)
    {
        memcpy(buf, m_data.data() + m_currentOffset, size);
        m_currentOffset += size;
    }

    return size;
}

Result<> PackageAuxMetaDataImpl::seek(off_t position)
{
    if ((position < 0) || (static_cast<size_t>(position) > m_data.size()))
        return Error(ErrorCode::InvalidArgument, "Seek position out of bounds");

    m_currentOffset = static_cast<size_t>(position);
    return {};
}

Result<std::vector<uint8_t>> PackageAuxMetaDataImpl::readAll()
{
    return m_data;
}
