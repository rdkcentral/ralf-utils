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

#include "LibarchiveFileReader.h"
#include "core/LogMacros.h"

#include <fcntl.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

LibarchiveFileReader::LibarchiveFileReader(int archiveFd, size_t archiveSize, ArchiveFormats requiredFormats,
                                           bool autoCloseFd, LIBRALF_NS::Error *_Nullable error)
    : m_fd(archiveFd)
    , m_size(static_cast<la_int64_t>(archiveSize))
    , m_requiredFormats(requiredFormats)
    , m_autoCloseFd(autoCloseFd)
    , m_offset(0)
    , m_buffer(4096, 0x00)
{
    // Sanity check
    if ((archiveFd < 0) || (archiveSize == 0))
    {
        if (error)
            error->assign(ErrorCode::InvalidArgument, "Invalid file descriptor or size");

        m_fd = -1;
        m_size = 0;
        return;
    }

    // Try and open the archive
    auto result = openArchive();
    if (!result)
    {
        if (error)
            *error = result.error();

        m_fd = -1;
        m_size = 0;
        return;
    }

    m_archive = result.value();
}

LibarchiveFileReader::~LibarchiveFileReader()
{
    if (m_archive)
    {
        archive_read_free(m_archive);
        m_archive = nullptr;
    }

    if (m_autoCloseFd && (m_fd >= 0))
    {
        if (close(m_fd) != 0)
            logSysError(errno, "Failed to close archive fd");
    }
}

// -------------------------------------------------------------------------
/*!
    \internal

    Creates a new libarchive archive reader object and 'opens' if by setting
    the read, seek and skip callbacks.

 */
Result<struct archive *> LibarchiveFileReader::openArchive()
{
    auto result = createArchiveReader(m_requiredFormats);
    if (!result)
        return result.error();

    auto archive = result.value();

    // Add the seek callback to allow us to seek in the archive
    if (archive_read_set_seek_callback(archive, &LibarchiveFileReader::seekCallback) != ARCHIVE_OK)
        logWarning("Failed to set seek callback - %s", archive_error_string(archive));

    // We don't use archive_read_open_fd as we want to use posix `pread` rather than just `read` so
    // the file offset is not changed.  This allows use to have multiple concurrent libarchive readers
    // on the same file descriptor.
    if (archive_read_open2(archive, static_cast<void *>(this), nullptr, &LibarchiveFileReader::readCallback,
                           &LibarchiveFileReader::skipCallback, nullptr) != ARCHIVE_OK)
    {
        Error error =
            Error::format(ErrorCode::ArchiveError, "Failed to open archive - %s", archive_error_string(archive));
        archive_free(archive);
        return error;
    }

    return Ok(archive);
}

// -------------------------------------------------------------------------
/*!
    Returns the file descriptor of the archive object.  The file descriptor
    is still owned by this object, and will be closed when the object is
    destroyed, unless autoCloseFd was set to false at construction time.

    Callers should not close the file descriptor, if they want to use the
    file descriptor they should dup it first.

 */
int LibarchiveFileReader::fd() const
{
    return m_fd;
}

// -------------------------------------------------------------------------
/*!
    Rewinds back to the start of the archive object.  This will invalidate
    all the current reading.

    With libarchive you cannot rewind the archive object, so we need close
    the current archive object and re-open it.  This is a bit of a pain, but
    not much we can do about it.
 */
int LibarchiveFileReader::rewind()
{
    // Close the current archive object
    if (m_archive)
    {
        archive_read_free(m_archive);
        m_archive = nullptr;
    }

    // Reset the offset
    m_offset = 0;

    // Re-open the archive object
    auto result = openArchive();
    if (!result)
        return ARCHIVE_FATAL;

    m_archive = result.value();

    return ARCHIVE_OK;
}

// -------------------------------------------------------------------------
/*!
    Creates a copy of the archive reader object.  This cloned reader is
    opened back at the start of the archive and is independent of the original
    archive.

    All clones duplicate the file descriptor, regardless of the autoCloseFd
    argument passed at construction time.

    \warning Do not support clone() for stream reading.
 */
std::unique_ptr<LibarchiveFileReader> LibarchiveFileReader::clone() const
{
    if (m_fd < 0)
    {
        logWarning("Cannot clone archive reader - invalid file descriptor or streaming archive reader");
        return nullptr;
    }

    int duppedFd = fcntl(m_fd, F_DUPFD_CLOEXEC, 3);
    if (duppedFd < 0)
    {
        logSysError(errno, "Failed to dup archive fd");
        return nullptr;
    }

    return std::make_unique<LibarchiveFileReader>(duppedFd, m_size, m_requiredFormats, true, nullptr);
}

la_ssize_t LibarchiveFileReader::readCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData,
                                              const void **outBuffer)
{
    (void)aArchive;

    auto *self = reinterpret_cast<LibarchiveFileReader *>(userData);

    *outBuffer = self->m_buffer.data();
    ssize_t rd = TEMP_FAILURE_RETRY(pread(self->m_fd, self->m_buffer.data(), self->m_buffer.size(), self->m_offset));
    if (rd < 0)
        archive_set_error(aArchive, errno, "Error reading package fd");
    else
        self->m_offset += rd;

    return rd;
}

la_int64_t LibarchiveFileReader::skipCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData, la_int64_t skip)
{
    (void)aArchive;

    auto *self = reinterpret_cast<LibarchiveFileReader *>(userData);

    if ((self->m_offset + skip) > self->m_size)
        skip = self->m_size - self->m_offset;

    // round down to a page size
    skip = (skip / 4096) * 4096;
    self->m_offset += skip;

    return skip;
}

la_int64_t LibarchiveFileReader::seekCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData,
                                              la_int64_t offset, int whence)
{
    (void)aArchive;

    auto *self = reinterpret_cast<LibarchiveFileReader *>(userData);

    if (whence == SEEK_SET)
        self->m_offset = offset;
    else if (whence == SEEK_CUR)
        self->m_offset += offset;
    else if (whence == SEEK_END)
        self->m_offset = self->m_size + offset;

    if (self->m_offset < 0)
        self->m_offset = 0;
    else if (self->m_offset > self->m_size)
        self->m_offset = self->m_size;

    return self->m_offset;
}

int LibarchiveFileReader::nextHeader(archive_entry **_Nullable entry)
{
    if (!m_archive)
        return ARCHIVE_FATAL;
    if (!entry)
        return ARCHIVE_FAILED;

    return archive_read_next_header(m_archive, entry);
}

ssize_t LibarchiveFileReader::readData(void *_Nullable buf, size_t size)
{
    if (!m_archive)
        return ARCHIVE_FATAL;

    return archive_read_data(m_archive, buf, size);
}

int LibarchiveFileReader::readDataBlock(const void **_Nullable buf, size_t *_Nullable size, int64_t *_Nullable offset)
{
    if (!m_archive)
        return ARCHIVE_FATAL;

    return archive_read_data_block(m_archive, buf, size, offset);
}

off_t LibarchiveFileReader::seekData(off_t offset, int whence)
{
    if (!m_archive)
        return -1;

    return archive_seek_data(m_archive, static_cast<la_int64_t>(offset), whence);
}

const char *LibarchiveFileReader::errorString()
{
    if (!m_archive)
        return "Invalid archive object";
    else
        return archive_error_string(m_archive);
}

bool LibarchiveFileReader::isNull() const
{
    return (m_archive == nullptr);
}

ArchiveFormat LibarchiveFileReader::format() const
{
    auto format = archiveFormat(m_archive);
    if (format == ArchiveFormat::Zip)
    {
        if ((m_requiredFormats & (ArchiveFormats::Zip | ArchiveFormats::ZipStream)) == ArchiveFormats::ZipStream)
            return ArchiveFormat::ZipStream;
    }

    return format;
}

int64_t LibarchiveFileReader::headerOffset() const
{
    if (!m_archive)
        return -1;

    return archive_read_header_position(m_archive);
}
