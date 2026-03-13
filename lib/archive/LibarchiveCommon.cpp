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

#include "LibarchiveCommon.h"
#include "core/LogMacros.h"

#include <atomic>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

#define WARN_UNSUPPORTED_FORMAT_ONCE(rc, format)                                                                       \
    if ((rc) != ARCHIVE_OK)                                                                                            \
    {                                                                                                                  \
        static std::atomic<bool> logged{ false };                                                                      \
        if (!logged.exchange(true))                                                                                    \
            logWarning("Libarchive does not support " #format);                                                        \
    }

// -------------------------------------------------------------------------
/*!
    \internal

    Creates a libarchive archive object for reading, with support for the
    specified archive formats if available.

    If the requested formats are not supported by libarchive, a warning will
    be logged once, but the archive object will still be created.  It is up to
    the caller to check if the archive object can actually read the required
    formats.

 */
Result<struct archive *> entos::ralf::archive::createArchiveReader(ArchiveFormats formats)
{
    // Create a new archive object for reading
    auto *archive = archive_read_new();
    if (!archive)
    {
        return Error(std::error_code(ENOMEM, std::system_category()), "Failed to create archive reader");
    }

    // Set the archive format and filters
    int rc;
    if ((formats & (ArchiveFormat::Tarball | ArchiveFormat::TarballGzip | ArchiveFormat::TarballZstd)) !=
        static_cast<ArchiveFormat>(0))
    {
        if ((formats & ArchiveFormat::TarballGzip) == ArchiveFormat::TarballGzip)
        {
            rc = archive_read_support_filter_gzip(archive);
            WARN_UNSUPPORTED_FORMAT_ONCE(rc, gzip compressed tarballs)
        }
        if ((formats & ArchiveFormat::TarballZstd) == ArchiveFormat::TarballZstd)
        {
            rc = archive_read_support_filter_zstd(archive);
            WARN_UNSUPPORTED_FORMAT_ONCE(rc, zstd compressed tarballs)
        }

        archive_read_support_format_gnutar(archive);
        rc = archive_read_support_format_tar(archive);
        WARN_UNSUPPORTED_FORMAT_ONCE(rc, tarballs)
    }

    if ((formats & ArchiveFormat::Zip) == ArchiveFormat::Zip)
    {
        rc = archive_read_support_filter_compress(archive);
        WARN_UNSUPPORTED_FORMAT_ONCE(rc, zip compression)

        // "Standard" zip reader, supports both streamable and seekable zip files, this gives us the most flexibility
        // and supports symlinks and unix permissions.
        rc = archive_read_support_format_zip(archive);
        WARN_UNSUPPORTED_FORMAT_ONCE(rc, zip files)
    }

    if ((formats & ArchiveFormat::ZipStream) == ArchiveFormat::ZipStream)
    {
        rc = archive_read_support_filter_compress(archive);
        WARN_UNSUPPORTED_FORMAT_ONCE(rc, zip compression)

        // Streamable zip reader only, this supports reading zip files as a stream from beginning to end. The reason
        // we use this is to work around a bug in the seekable zip reader. The bug is with the
        // archive_read_header_position API which we need to get the file offset of image entries for mounting.
        // The seekable reader incorrectly returns the offset of the previous entry's data block, not the current
        // entry's header offset.
        //
        // Streamable zip reader does not support symlinks or unix permissions, but RALF/OCI packages don't use these
        // features, all files are regular files with 0644 or 0755 permissions.
        rc = archive_read_support_format_zip_streamable(archive);
        WARN_UNSUPPORTED_FORMAT_ONCE(rc, zip files)
    }

    return Ok(archive);
}

// -------------------------------------------------------------------------
/*!
    Helper function to determine the archive format of the given archive.

    \warning This only returns a valid format if at least one entry
    header has been read from the archive. If no entries have been read,
    it will return ArchiveFormat::Unknown.

    libarchive can support multiple compression formats in a chain, this
    function tries to determine the format of the archive by assuming there
    is actually a single format archive.

*/
ArchiveFormat entos::ralf::archive::archiveFormat(struct archive *aArchive)
{
    if (!aArchive)
        return ArchiveFormat::Unknown;

    const int formatBase = (archive_format(aArchive) & ARCHIVE_FORMAT_BASE_MASK);
    if (formatBase == ARCHIVE_FORMAT_TAR)
    {
        const int filterCount = archive_filter_count(aArchive);
        for (int i = 0; i < filterCount; ++i)
        {
            int filterCode = archive_filter_code(aArchive, i);
            if (filterCode == ARCHIVE_FILTER_GZIP)
                return ArchiveFormat::TarballGzip;
            else if (filterCode == ARCHIVE_FILTER_ZSTD)
                return ArchiveFormat::TarballZstd;
            else if (filterCode != ARCHIVE_FILTER_NONE)
                return ArchiveFormat::Unknown;
        }

        return ArchiveFormat::Tarball;
    }
    else if (formatBase == ARCHIVE_FORMAT_ZIP)
    {
        return ArchiveFormat::Zip;
    }

    return ArchiveFormat::Unknown;
}
