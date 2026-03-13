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

#include "LibarchiveStreamReader.h"
#include "core/LogMacros.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

LibarchiveStreamReader::LibarchiveStreamReader(ReadFunction readFunction, SkipFunction skipFunction,
                                               ArchiveFormats requiredFormats, LIBRALF_NS::Error *_Nullable error)
    : m_streamReadFn(std::move(readFunction))
    , m_streamSkipFn(std::move(skipFunction))
{
    if (error)
        error->clear();

    // Try and open the archive
    auto result = createArchiveReader(requiredFormats);
    if (!result)
    {
        if (error)
            *error = result.error();
        return;
    }

    m_archive = result.value();

    // Skip is optional, so we can pass nullptr if we don't have a skip function
    archive_skip_callback *skipper = m_streamSkipFn ? &LibarchiveStreamReader::skipCallback : nullptr;

    // We don't use archive_read_open_fd as we want to use posix `pread` rather than just `read` so
    // the file offset is not changed.  This allows use to have multiple concurrent libarchive readers
    // on the same file descriptor.
    if (archive_read_open2(m_archive, static_cast<void *>(this), nullptr, &LibarchiveStreamReader::readCallback,
                           skipper, nullptr) != ARCHIVE_OK)
    {
        if (error)
            *error =
                Error::format(ErrorCode::ArchiveError, "Failed to open archive - %s", archive_error_string(m_archive));

        archive_free(m_archive);
        m_archive = nullptr;
    }
}

LibarchiveStreamReader::~LibarchiveStreamReader()
{
    if (m_archive)
    {
        archive_read_free(m_archive);
        m_archive = nullptr;
    }
}

ArchiveFormat LibarchiveStreamReader::format() const
{
    return archiveFormat(m_archive);
}

la_ssize_t LibarchiveStreamReader::readCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData,
                                                const void *_Nullable *_Nonnull outBuffer)
{
    (void)aArchive;

    auto *self = reinterpret_cast<LibarchiveStreamReader *>(userData);

    return self->m_streamReadFn(outBuffer);
}

la_int64_t LibarchiveStreamReader::skipCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData,
                                                la_int64_t skip)
{
    (void)aArchive;

    auto *self = reinterpret_cast<LibarchiveStreamReader *>(userData);

    return self->m_streamSkipFn(skip);
}

int LibarchiveStreamReader::nextHeader(archive_entry **_Nullable entry)
{
    if (!m_archive)
        return ARCHIVE_FATAL;

    return archive_read_next_header(m_archive, entry);
}

ssize_t LibarchiveStreamReader::readData(void *_Nullable buf, size_t size)
{
    if (!m_archive)
        return ARCHIVE_FATAL;

    return archive_read_data(m_archive, buf, size);
}

int LibarchiveStreamReader::readDataBlock(const void **_Nullable buf, size_t *_Nullable size, int64_t *_Nullable offset)
{
    if (!m_archive)
        return ARCHIVE_FATAL;

    return archive_read_data_block(m_archive, buf, size, offset);
}

const char *LibarchiveStreamReader::errorString()
{
    if (!m_archive)
        return "Invalid archive object";
    else
        return archive_error_string(m_archive);
}

bool LibarchiveStreamReader::isNull() const
{
    return (m_archive == nullptr);
}
