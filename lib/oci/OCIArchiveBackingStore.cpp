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

#include "OCIArchiveBackingStore.h"
#include "OCIMappableFile.h"
#include "core/CryptoDigestBuilder.h"
#include "core/LogMacros.h"

#include <cstring>

#include <fcntl.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

/// The maximum size of a file within the package that we allow to be mapped into memory
#define MAX_MMAP_FILE_SIZE (2LL * 1024 * 1024 * 1024)

// clang-format off
/// Macro for dealing with stuff on 512 boundaries
#define ROUND_UP(n, s) ((((n) + (s) - 1) / (s)) * (s))
// clang-format on

// -------------------------------------------------------------------------
/*!
    \class BufferReader
    \brief Instance of IOCIFileReader that reads from a buffer.

    This is used for cached small files from the backing store.  It just
    wraps a std::vector<uint8_t> and provides the IOCIFileReader interface.

 */
class BufferReader final : public IOCIFileReader
{
public:
    explicit BufferReader(std::shared_ptr<const std::vector<uint8_t>> buffer)
        : m_buffer(std::move(buffer))
        , m_offset(0)
    {
    }

    ~BufferReader() final = default;

    ssize_t read(void *buf, size_t size) override
    {
        if (m_offset >= static_cast<ssize_t>(m_buffer->size()))
            return 0;

        size_t bytesToRead = std::min<size_t>(size, m_buffer->size() - m_offset);
        std::memcpy(buf, m_buffer->data() + m_offset, bytesToRead);
        m_offset += static_cast<ssize_t>(bytesToRead);

        return static_cast<ssize_t>(bytesToRead);
    }

    int64_t seek(int64_t offset, int whence) override
    {
        if (whence == SEEK_SET)
            m_offset = static_cast<ssize_t>(offset);
        else if (whence == SEEK_CUR)
            m_offset += static_cast<ssize_t>(offset);
        else if (whence == SEEK_END)
            m_offset = static_cast<int64_t>(m_buffer->size()) + offset;

        if (m_offset < 0)
            m_offset = 0;
        else if (m_offset > static_cast<int64_t>(m_buffer->size()))
            m_offset = static_cast<int64_t>(m_buffer->size());

        return m_offset;
    }

    int64_t size() const override { return static_cast<int64_t>(m_buffer->size()); }

private:
    std::shared_ptr<const std::vector<uint8_t>> m_buffer;
    int64_t m_offset;
};

// -------------------------------------------------------------------------
/*!
    \class ArchiveEntryReader
    \brief Instance of IOCIFileReader that wraps an archive entry.

    This object takes a the archive reader object and the details on the
    current entry, it then allows you to linearly read the contents of the
    archive entry.

 */
class ArchiveEntryReader final : public IOCIFileReader
{
public:
    ArchiveEntryReader(std::unique_ptr<LibarchiveFileReader> &&archive, archive_entry *const entry)
        : m_archive(std::move(archive))
        , m_size(archive_entry_size(entry))
    {
    }

    ~ArchiveEntryReader() final = default;

    ssize_t read(void *buf, size_t size) override { return m_archive->readData(buf, size); }

    int64_t seek(int64_t offset, int whence) override
    {
        return m_archive->seekData(static_cast<off_t>(offset), whence);
    }

    int64_t size() const override { return m_size; }

private:
    std::unique_ptr<LibarchiveFileReader> m_archive;
    int64_t m_size;
};

// -------------------------------------------------------------------------
/*!
    \static

    Opens an archive and creates a mapping store object for it.

 */
Result<std::shared_ptr<OCIArchiveBackingStore>> OCIArchiveBackingStore::open(int archiveFd, bool enableCache)
{
    // dup the file descriptor so we have our own copy
    int dupedFd = fcntl(archiveFd, F_DUPFD_CLOEXEC, 3);
    if (dupedFd < 0)
        return Error(std::error_code(errno, std::system_category()), "Failed to dup archive fd");

    struct stat st = {};
    if (fstat(dupedFd, &st) < 0)
    {
        Error error(std::error_code(errno, std::system_category()), "Failed to get archive size");
        close(dupedFd);
        return error;
    }

    // The type of archives we support
    const ArchiveFormats formats = ArchiveFormat::Tarball | ArchiveFormat::TarballZstd | ArchiveFormat::TarballGzip |
                                   ArchiveFormat::ZipStream;

    // Try and open the file backed archive
    Error error;
    auto archive = std::make_unique<LibarchiveFileReader>(dupedFd, st.st_size, formats, true, &error);
    if (!archive || archive->isNull())
    {
        close(dupedFd);
        return Error::format(ErrorCode::PackageContentsInvalid, "%s", error.what());
    }

    return std::shared_ptr<OCIArchiveBackingStore>(
        new OCIArchiveBackingStore(std::move(archive), st.st_size, enableCache));
}

// -------------------------------------------------------------------------
/*!
    \internal

 */
OCIArchiveBackingStore::OCIArchiveBackingStore(std::unique_ptr<LibarchiveFileReader> &&archive, int64_t size,
                                               bool enableCache)
    : m_archive(std::move(archive))
    , m_size(size)
{
    if (enableCache)
        cacheSmallFiles();
}

int64_t OCIArchiveBackingStore::size() const
{
    return m_size;
}

// -------------------------------------------------------------------------
/*!
    Returns \c true if the archive is uncompressed tar or zip format. If a
    compressed tar format (gzip or zstd) is used then this returns \c false,
    because the files within the archive cannot be mapped into memory (without
    first uncompressing the entire archive).

    \note Zip files can contain uncompressed entries, which means they can be
    mapped into memory, however this function doesn't check the individual
    entries, it just assumes that zip files are mountable.
*/
bool OCIArchiveBackingStore::supportsMountableFiles() const
{
    const auto format = m_archive->format();
    return (format == ArchiveFormat::Tarball) || (format == ArchiveFormat::ZipStream);
}

// -------------------------------------------------------------------------
/*!
    \internal
    \static

    Helper function to read the entire contents of the file in the archive
    and returns it as buffer.

    If the file is larger than \a maxSize then an error result is returned.
 */
Result<std::vector<uint8_t>>
OCIArchiveBackingStore::readArchiveFileEntry(const std::unique_ptr<LibarchiveFileReader> &archive,
                                             archive_entry *const entry, const int64_t maxSize)
{
    const auto size = archive_entry_size(entry);
    if ((size < 0) || (size > maxSize))
        return Error(ErrorCode::PackageFileTooLarge, "File is too large");

    std::vector<uint8_t> buffer;
    buffer.reserve(size);

    char buf[4096];
    ssize_t len = archive->readData(buf, sizeof(buf));
    while (len > 0)
    {
        if ((static_cast<ssize_t>(buffer.size()) + len) > size)
            return Error(ErrorCode::PackageFileTooLarge, "File is too large");

        buffer.insert(buffer.end(), buf, buf + len);
        len = archive->readData(buf, sizeof(buf));
    }

    return buffer;
}

// -------------------------------------------------------------------------
/*!
    Builds a cache of the small files in the archive.

    OCI archives consist of a number of small JSON files, which are used
    in a hierarchical way to describe the contents. In addition libarchive only
    supports reading an archive linearly, no random access, so as a first pass
    we walk through the archive and cache any small files. Hopefully this
    avoids having to process the same archive multiple times.

*/
void OCIArchiveBackingStore::cacheSmallFiles()
{
    // Any file smaller than this will be cached, if space available in the cache
    const ssize_t cacheThreshold = 16 * 1024;

    // The maximum space available in the cache
    ssize_t cacheSpace = 64 * 1024;

    // Process the archive and cache any small files
    archive_entry *entry = nullptr;
    while (m_archive->nextHeader(&entry) == ARCHIVE_OK)
    {
        const char *entryPath = archive_entry_pathname(entry);
        const int64_t size = archive_entry_size(entry);
        const mode_t type = (archive_entry_filetype(entry) & AE_IFMT);
        if ((type != AE_IFREG) || !entryPath || (size <= 0))
            continue;

        if ((size <= cacheThreshold) && (size <= cacheSpace) && (m_cache.count(entryPath) == 0))
        {
            auto contents = readArchiveFileEntry(m_archive, entry, size);
            if (contents)
            {
                m_cache.emplace(entryPath, std::make_shared<std::vector<uint8_t>>(std::move(contents.value())));
                cacheSpace -= static_cast<ssize_t>(contents.value().size());
            }
        }
    }
}

Result<std::vector<uint8_t>> OCIArchiveBackingStore::readFile(const std::filesystem::path &path, size_t maxSize) const
{
    // Check if the file is in the cache
    auto it = m_cache.find(path);
    if (it != m_cache.end())
    {
        if (it->second->size() > maxSize)
            return Error(ErrorCode::PackageFileTooLarge, "File is too large");

        return *it->second;
    }

    // If not in the cache, read it from the archive
    auto archive = m_archive->clone();
    if (!archive)
        return Error(ErrorCode::InternalError, "Failed to clone archive");

    archive_entry *entry = nullptr;
    while (archive->nextHeader(&entry) == ARCHIVE_OK)
    {
        const char *entryPath = archive_entry_pathname(entry);
        const int64_t size = archive_entry_size(entry);
        const mode_t type = (archive_entry_filetype(entry) & AE_IFMT);
        if ((type != AE_IFREG) || !entryPath || (size < 0))
            continue;

        // Check if the entry we're looking for
        if (path == entryPath)
        {
            if (size > static_cast<int64_t>(maxSize))
                return Error::format(ErrorCode::PackageFileTooLarge, "File '%s' is too large", path.c_str());

            return readArchiveFileEntry(archive, entry, size);
        }
    }

    return Error::format(ErrorCode::PackageContentsInvalid, "File '%s' not found in package", path.c_str());
}

Result<std::unique_ptr<IOCIFileReader>> OCIArchiveBackingStore::getFile(const std::filesystem::path &path) const
{
    // Check if the file is in the cache, if so just return a reader that reads the buffer
    auto it = m_cache.find(path);
    if (it != m_cache.end())
    {
        return std::make_unique<BufferReader>(it->second);
    }

    // If not in the cache, read it from the archive
    auto archive = m_archive->clone();
    if (!archive)
        return Error(ErrorCode::InternalError, "Failed to clone archive");

    archive_entry *entry = nullptr;
    while (archive->nextHeader(&entry) == ARCHIVE_OK)
    {
        const char *entryPath = archive_entry_pathname(entry);
        const mode_t type = (archive_entry_filetype(entry) & AE_IFMT);
        if ((type != AE_IFREG) || !entryPath)
            continue;

        // Check if the entry we're looking for
        if (path == entryPath)
        {
            return std::make_unique<ArchiveEntryReader>(std::move(archive), entry);
        }
    }

    return Error::format(ErrorCode::PackageContentsInvalid, "File '%s' not found in package", path.c_str());
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Attempts to get the tar file offset and size of the archive entry, the values
    returned are suitable for use to read from the archive file directly. This
    only works for uncompressed tar archives.

    Included in the returned MappableFile object is the file descriptor of the
    archive, it is the caller's responsibility to close this file descriptor.

    The code works by first getting the file offset of the archive entry header,
    then it manually walks through the tar headers to find the data offset.

 */
Result<std::unique_ptr<IOCIMappableFile>>
OCIArchiveBackingStore::getTarArchiveEntryMappableFile(const std::unique_ptr<LibarchiveFileReader> &archive,
                                                       archive_entry *const entry)
{
    // Get the file descriptor of the archive
    int archiveFd = archive->fd();
    if (archiveFd < 0)
        return Error(ErrorCode::InternalError, "Failed to get archive file descriptor");

    // Get the size of the entry
    const la_int64_t entrySize = archive_entry_size(entry);
    if ((entrySize <= 0) || (entrySize > MAX_MMAP_FILE_SIZE))
        return Error(ErrorCode::PackageFileTooLarge, "File is too large");

    // Get the current offset of the header in the archive
    la_int64_t offset = archive->headerOffset();
    if ((offset < 0) || (offset >= (std::numeric_limits<off_t>::max() - entrySize)))
        return Error(ErrorCode::InternalError, "Failed to get archive header offset or out of bounds");
    if (offset % 512 != 0)
        return Error(ErrorCode::InternalError, "Archive header offset is not aligned to 512 bytes");

    // Now we actually need the data offset, not the header offset and libarchive does not provide a way to get that,
    // so we manually walk over the tar header(s) to find the data offset.
    //
    // note: https://mort.coffee/home/tar/ has a good explanation of the tar format and the various header types.

    struct TarFileHeader
    {
        char filePath[100];
        char fileMode[8];
        char ownerUserId[8];
        char ownerGroupId[8];
        char fileSize[12];
        char fileMTime[12];
        char headerChecksum[8];
        char fileType;
        char linkPath[100];
        char padding[255];
    } __attribute__((packed));
    static_assert(sizeof(TarFileHeader) == 512, "TarFileHeader must be 512 bytes");

    TarFileHeader header = {};
    while (true)
    {
        // Read the tar header at the current offset
        if (TEMP_FAILURE_RETRY(pread(archiveFd, &header, sizeof(TarFileHeader), offset)) != sizeof(TarFileHeader))
        {
            logSysError(errno, "Failed to read tar header at offset %" PRId64, static_cast<int64_t>(offset));
            return Error(std::error_code(errno, std::system_category()), "Failed to read tar header");
        }

        // Get the size of the data block from the header, which is oddly stored in octal format
        char fileSizeOctal[13] = {};
        memcpy(fileSizeOctal, header.fileSize, sizeof(header.fileSize));
        fileSizeOctal[12] = '\0';

        const unsigned long dataSize = strtoul(fileSizeOctal, nullptr, 8);
        if ((dataSize == 0) || (dataSize > MAX_MMAP_FILE_SIZE))
        {
            return Error(ErrorCode::PackageContentsInvalid, "Found invalid file size in tar header");
        }

        // The file type determines how we handle the entry
        if (header.fileType == '0' || header.fileType == '\0')
        {
            // Sanity check the offset and size
            const uint64_t imageOffset = offset + 512;
            if ((imageOffset + entrySize) < imageOffset)
                return Error(ErrorCode::PackageContentsInvalid, "Tar entry size overflow");
            if ((imageOffset + entrySize) > static_cast<uint64_t>(std::numeric_limits<off_t>::max()))
                return Error(ErrorCode::PackageContentsInvalid, "Tar entry size out of bounds");

            // Dup the file descriptor so we can return in the MappableFile object and the caller is responsible for
            // closing it.
            int duppedFd = fcntl(archiveFd, F_DUPFD_CLOEXEC, 3);
            if (duppedFd < 0)
            {
                return Error(std::error_code(errno, std::system_category()), "Failed to dup archive fd");
            }

            // Regular file header, which means that the after the header there is a data block
            return std::make_unique<OCIMappableFile>(duppedFd, imageOffset, entrySize);
        }
        else if ((header.fileType == 'x') || (header.fileType == 'g') || (header.fileType == 'K') ||
                 (header.fileType == 'L'))
        {
            // These are PAX  or GNU extended headers, we just skip these to get to the real entry
            offset += static_cast<la_int64_t>(512 + ROUND_UP(dataSize, 512));
        }
        else
        {
            // Any other header type would indicate a non-file archive entry, or something we don't support
            return Error::format(ErrorCode::PackageContentsInvalid, "Unsupported file type (0x%02x) in tar archive",
                                 header.fileType);
        }
    }
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Attempts to get the zip file offset and size of the archive entry, the values
    returned are suitable for use to read from the archive file directly. This
    only works if the zip entry is stored (not compressed).

    Included in the returned MappableFile object is the file descriptor of the
    archive, it is the caller's responsibility to close this file descriptor.

    The code works by first getting the file offset of the zip entry file header,
    then it manually reads the header structure to check the compression type
    and the data offset.

 */
Result<std::unique_ptr<IOCIMappableFile>>
OCIArchiveBackingStore::getZipArchiveEntryMappableFile(const std::unique_ptr<LibarchiveFileReader> &archive,
                                                       archive_entry *const entry)
{
    // Get the file descriptor of the archive
    int archiveFd = archive->fd();
    if (archiveFd < 0)
        return Error(ErrorCode::InternalError, "Failed to get archive file descriptor");

    // Get the size of the entry
    const la_int64_t entrySize = archive_entry_size(entry);
    if ((entrySize <= 0) || (entrySize > MAX_MMAP_FILE_SIZE))
        return Error(ErrorCode::PackageFileTooLarge, "File is too large");

    // Get the offset of the zip header in the archive, this only works for the streaming zip reader
    const la_int64_t offset = archive->headerOffset();
    if ((offset < 0) || (offset >= (std::numeric_limits<off_t>::max() - entrySize)))
        return Error(ErrorCode::InternalError, "Failed to get archive header offset or out of bounds");

    // Now we actually need the data offset, not the header offset and libarchive does not provide a way to get that,
    // so we manually read the zip file header to find the data offset.

    // All values are store in little-endian format, and this library only supports little-endian platforms
    struct ZipFileHeader
    {
        uint32_t magic;
        uint16_t version;
        uint16_t flags;
        uint16_t compression;
        uint16_t modTime;
        uint16_t modDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t fileNameLength;
        uint16_t extraFieldLength;
        // file name (variable size)
        // extra field (variable size)
    } __attribute__((packed));
    static_assert(sizeof(ZipFileHeader) == 30, "ZipFileHeader must be 30 bytes");

    // Read the zip local file header at the current offset
    ZipFileHeader header = {};
    if (TEMP_FAILURE_RETRY(pread(archiveFd, &header, sizeof(ZipFileHeader), offset)) != sizeof(ZipFileHeader))
    {
        logSysError(errno, "Failed to read zip entry header at offset %" PRId64, static_cast<int64_t>(offset));
        return Error(std::error_code(errno, std::system_category()), "Failed to read zip file entry header");
    }

    // Check the magic number
    if (header.magic != 0x04034b50)
    {
        return Error(ErrorCode::PackageContentsInvalid, "Invalid zip file header magic");
    }

    // Check the compression type, we only support stored (no compression)
    if (header.compression != 0)
    {
        return Error::format(ErrorCode::PackageContentsInvalid,
                             "Unsupported compression type (0x%04x) in zip archive for mapping", header.compression);
    }

    // Check the compressed size matches the uncompressed size
    if (header.compressedSize != header.uncompressedSize)
    {
        return Error::format(ErrorCode::PackageContentsInvalid,
                             "Mismatched compressed/uncompressed size (%u/%u) in zip archive for mapping",
                             header.compressedSize, header.uncompressedSize);
    }

    if (header.uncompressedSize > static_cast<uint32_t>(MAX_MMAP_FILE_SIZE))
        return Error(ErrorCode::PackageFileTooLarge, "Image file is too large");
    if (header.uncompressedSize == 0)
        return Error(ErrorCode::PackageContentsInvalid, "Image file is empty");

    // Get the size of the header block
    size_t headerSize = sizeof(ZipFileHeader);
    headerSize += header.fileNameLength;
    headerSize += header.extraFieldLength;

    // And from that we can calculate the data offset
    const uint64_t imageSize = header.uncompressedSize;
    if (imageSize != static_cast<uint64_t>(entrySize))
        return Error(ErrorCode::PackageContentsInvalid, "Zip entry size mismatch");

    // Calculate the image offset
    const uint64_t imageOffset = static_cast<uint64_t>(offset) + headerSize;
    if ((imageOffset == 0) || (imageOffset >= (std::numeric_limits<off_t>::max() - imageSize)))
        return Error(ErrorCode::PackageContentsInvalid, "Failed to get zip entry offset or out of bounds");
    if ((imageOffset + imageSize) < imageOffset)
        return Error(ErrorCode::PackageContentsInvalid, "Zip entry size overflow");
    if ((imageOffset + imageSize) > static_cast<uint64_t>(std::numeric_limits<off_t>::max()))
        return Error(ErrorCode::PackageContentsInvalid, "Zip entry size out of bounds");

    // Dup the file descriptor so we can return in the MappableFile object and the caller is responsible for
    // closing it.
    int duppedFd = fcntl(archiveFd, F_DUPFD_CLOEXEC, 3);
    if (duppedFd < 0)
    {
        return Error(std::error_code(errno, std::system_category()), "Failed to dup archive fd");
    }

    // Regular file header, which means that the after the header there is a data block
    return std::make_unique<OCIMappableFile>(duppedFd, imageOffset, imageSize);
}

Result<std::unique_ptr<IOCIMappableFile>> OCIArchiveBackingStore::getMappableFile(const std::filesystem::path &path) const
{
    // Clone the archive so don't mess with anyone else using it
    auto archive = m_archive->clone();
    if (!archive)
        return Error(ErrorCode::InternalError, "Failed to clone archive");

    // Walk through the archive and find the file
    archive_entry *entry = nullptr;
    while (archive->nextHeader(&entry) == ARCHIVE_OK)
    {
        const char *entryPath = archive_entry_pathname(entry);
        const mode_t type = (archive_entry_filetype(entry) & AE_IFMT);
        if ((type != AE_IFREG) || !entryPath || (path != entryPath))
            continue;

        // If the archive is not a plain uncompressed tarball then we cannot return an offset
        const auto format = archive->format();
        if (format == ArchiveFormat::Tarball)
            return getTarArchiveEntryMappableFile(archive, entry);
        else if (format == ArchiveFormat::ZipStream)
            return getZipArchiveEntryMappableFile(archive, entry);
        else
            return Error(ErrorCode::PackageContentsInvalid, "File in archive is not mappable");
    }

    return Error(ErrorCode::PackageContentsInvalid, "Failed to find file in archive");
}
