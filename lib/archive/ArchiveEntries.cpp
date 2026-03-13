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

#include "ArchiveEntries.h"
#include "ILibarchiveReader.h"
#include "core/LogMacros.h"
#include "core/Utils.h"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

/// The maximum size allowed for entries within a package.  This is to prevent out of memory errors
/// if the package container malicious data.
#define MAX_PACKAGE_ENTRY_SIZE (128 * 1024 * 1024)

static const std::filesystem::perms kDefaultDirectoryPerms =
    std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec |
    std::filesystem::perms::group_read | std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
    std::filesystem::perms::others_exec;

static const std::filesystem::perms kDefaultSymLinkPerms = std::filesystem::perms::owner_read |
                                                           std::filesystem::perms::owner_write |
                                                           std::filesystem::perms::group_read |
                                                           std::filesystem::perms::others_read;

static const std::filesystem::perms kDefaultExecutableFilePerms =
    std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec |
    std::filesystem::perms::group_read | std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
    std::filesystem::perms::others_exec;

static const std::filesystem::perms kDefaultFilePerms = std::filesystem::perms::owner_read |
                                                        std::filesystem::perms::owner_write |
                                                        std::filesystem::perms::group_read |
                                                        std::filesystem::perms::others_read;

std::unique_ptr<ArchiveDirectoryEntry> ArchiveDirectoryEntry::create(std::filesystem::path path, const time_t modTime)
{
    return std::make_unique<ArchiveDirectoryEntry>(std::move(path), modTime);
}

std::unique_ptr<ArchiveDirectoryEntry> ArchiveDirectoryEntry::create(const std::filesystem::path::iterator &pathBegin,
                                                                     const std::filesystem::path::iterator &pathEnd,
                                                                     const time_t modTime)
{
    std::filesystem::path path;
    for (auto it = pathBegin; it != pathEnd; ++it)
        path /= *it;
    path /= *pathEnd;

    return std::make_unique<ArchiveDirectoryEntry>(std::move(path), modTime);
}

ArchiveDirectoryEntry::ArchiveDirectoryEntry(std::filesystem::path &&path, const time_t modTime)
    : ArchiveBaseEntry(std::move(path), std::filesystem::file_type::directory, 0, kDefaultDirectoryPerms, modTime, 0, 0)
{
}

ssize_t ArchiveDirectoryEntry::read(void *buf, size_t size, Error *_Nullable error)
{
    if (error)
        error->assign(ErrorCode::NotSupported, "Cannot read from a directory entry");

    return -1;
}

Result<size_t> ArchiveDirectoryEntry::writeTo(int directoryFd, const size_t maxSize, Package::ExtractOptions options)
{
    (void)maxSize;

    auto result = checkExtraction(directoryFd, m_path, std::filesystem::file_type::directory, m_perms, options);
    if (!result)
        return result.error();

    if (result.value() == ExtractOperation::Extract)
    {
        if (mkdirat(directoryFd, m_path.c_str(), 0755) != 0)
        {
            return Error::format(std::error_code(errno, std::system_category()), "Failed to create directory '%s'",
                                 m_path.c_str());
        }

        // Set the file modification time
        struct timespec times[2];
        times[0].tv_sec = 0;
        times[0].tv_nsec = UTIME_OMIT;
        times[1].tv_sec = m_modTime;
        times[1].tv_nsec = 0;
        if (utimensat(directoryFd, m_path.c_str(), times, AT_SYMLINK_NOFOLLOW) != 0)
            logSysWarning(errno, "Failed to set modification time for '%s'", m_path.c_str());
    }

    return Ok(static_cast<size_t>(0));
}

std::unique_ptr<ArchiveSymlinkEntry>
ArchiveSymlinkEntry::create(const std::shared_ptr<ArchiveReader::SharedReadState> &readState, std::filesystem::path path,
                            const char *target, const time_t modTime, const std::optional<std::vector<uint8_t>> &digest)
{
    // Sanity check a target path was supplied
    if (!target || (target[0] == '\0'))
    {
        readState->error = Error::format(ErrorCode::PackageInvalidEntry,
                                         "Null symlink target for '%s' in archive entry", path.c_str());
        return nullptr;
    }

    // Verify the symlink target against the sha256 hash if supplied
    std::string targetStr(target);
    if (digest && (digest.value() != CryptoDigestBuilder::digest(CryptoDigestBuilder::Algorithm::Sha256, targetStr)))
    {
        readState->error =
            Error::format(ErrorCode::PackageSignatureInvalid, "Symlink '%s' in archive failed to verify", path.c_str());
        return nullptr;
    }

    return std::make_unique<ArchiveSymlinkEntry>(std::move(path), std::move(targetStr), modTime);
}

ArchiveSymlinkEntry::ArchiveSymlinkEntry(std::filesystem::path &&path, std::string &&target, time_t modTime)
    : ArchiveBaseEntry(std::move(path), std::filesystem::file_type::symlink, target.size(), kDefaultSymLinkPerms,
                       modTime, 0, 0)
    , m_target(std::move(target))
    , m_offset(0)
{
}

ssize_t ArchiveSymlinkEntry::read(void *buf, size_t size, Error *_Nullable error)
{
    if (error)
        error->clear();

    if (m_offset >= m_target.size())
        return 0;

    auto len = std::min<size_t>(size, m_target.size() - m_offset);
    memcpy(buf, m_target.data() + m_offset, len);
    m_offset += len;

    return static_cast<ssize_t>(len);
}

Result<size_t> ArchiveSymlinkEntry::writeTo(int directoryFd, const size_t maxSize, Package::ExtractOptions options)
{
    if (m_target.size() > maxSize)
        return Error::format(ErrorCode::PackageInvalidEntry, "Symlink target for '%s' is too long", m_path.c_str());

    auto result = checkExtraction(directoryFd, m_path, std::filesystem::file_type::symlink, m_perms, options);
    if (!result)
        return result.error();

    if (result.value() == ExtractOperation::Extract)
    {
        if (symlinkat(m_target.c_str(), directoryFd, m_path.c_str()) != 0)
        {
            return Error::format(std::error_code(errno, std::system_category()), "Failed to create symlink '%s'",
                                 m_path.c_str());
        }

        // Set the file modification time
        struct timespec times[2];
        times[0].tv_sec = 0;
        times[0].tv_nsec = UTIME_OMIT;
        times[1].tv_sec = m_modTime;
        times[1].tv_nsec = 0;
        if (utimensat(directoryFd, m_path.c_str(), times, AT_SYMLINK_NOFOLLOW) != 0)
            logSysWarning(errno, "Failed to set modification time for '%s'", m_path.c_str());
    }

    return Ok(m_target.size());
}

std::unique_ptr<ArchiveFileEntry>
ArchiveFileEntry::create(const std::shared_ptr<ArchiveReader::SharedReadState> &readState, std::filesystem::path path,
                         int64_t size, bool executable, const time_t modTime,
                         const std::optional<std::vector<uint8_t>> &digest)
{
    // Sanity check the size of the file is valid
    size = std::max<int64_t>(size, 0);
    if (size > MAX_PACKAGE_ENTRY_SIZE)
    {
        readState->error =
            Error::format(ErrorCode::PackageContentsInvalid, "'%s' file in archive is too large", path.c_str());
        return nullptr;
    }

    // Create the entry
    return std::make_unique<ArchiveFileEntry>(readState, std::move(path), static_cast<size_t>(size), executable,
                                              modTime, digest);
}

ArchiveFileEntry::ArchiveFileEntry(const std::shared_ptr<ArchiveReader::SharedReadState> &readState,
                                   std::filesystem::path &&path, size_t size, bool executable, time_t modTime,
                                   std::optional<std::vector<uint8_t>> digest)
    : ArchiveBaseEntry(std::move(path), std::filesystem::file_type::regular, size,
                       executable ? kDefaultExecutableFilePerms : kDefaultFilePerms, modTime, 0, 0)
    , m_readState(readState)
    , m_digest(std::move(digest))
    , m_index(readState->entryIndex)
    , m_eof(false)
{
}

// -------------------------------------------------------------------------
/*!
    \internal

    Checks if the parent reader object is still valid and still pointing to
    this entry.  If not then an error is set and nullptr is returned.

*/
std::shared_ptr<ArchiveReader::SharedReadState> ArchiveFileEntry::checkReadState(Error *_Nullable error)
{
    // Check if the read state is still valid
    auto readState = m_readState.lock();
    if (!readState)
    {
        if (error)
            error->assign(ErrorCode::InternalError, "Archive reader has been destroyed");
        return nullptr;
    }

    // Check if there has been an error reading the archive
    if (readState->error)
    {
        if (error)
            *error = readState->error;
        return nullptr;
    }

    // Check if the archive has moved on since this entry was created
    if (readState->endOfArchive || (readState->entryIndex != m_index))
    {
        if (error)
            error->assign(ErrorCode::InternalError, "Archive reader has moved to next entry");
        return nullptr;
    }

    return readState;
}

// -------------------------------------------------------------------------
/*!
    Performs a read of the archive entry, this behaves the same as a read(),
    however when reached the end of the entry the digest is calculated and
    compared to the expected value, if supplied.

 */
ssize_t ArchiveFileEntry::read(void *buf, size_t size, Error *_Nullable error)
{
    if (!buf)
    {
        if (error)
            error->assign(ErrorCode::InvalidArgument, "Null buffer supplied");
        return -1;
    }

    // Checks if the parent reader object is still valid and still pointing to this entry
    auto readState = checkReadState(error);
    if (!readState)
    {
        // Error already set
        return -1;
    }

    // Check if already read the entire entry
    if (m_eof)
    {
        if (error)
            error->clear();
        return 0;
    }

    // Can now finally read the data from the archive
    ssize_t rd = readState->archive->readData(buf, size);
    if (rd < 0)
    {
        readState->error = Error::format(ErrorCode::PackageContentsInvalid, "Error reading from archive - %s",
                                         readState->archive->errorString());
        if (error)
            *error = readState->error;

        return -1;
    }

    // Set the EOF flag if we've reached the end of the entry in the archive
    m_eof = (rd == 0);

    // If read some data then update the digest
    if (m_digest)
    {
        // If we don't have a digest builder then create one now
        if (!m_digestBuilder)
        {
            m_digestBuilder = std::make_unique<CryptoDigestBuilder>(CryptoDigestBuilder::Algorithm::Sha256);
            if (!m_digestBuilder)
            {
                readState->error =
                    Error::format(ErrorCode::InternalError, "Failed to create digest builder for archive entry");
                if (error)
                    *error = readState->error;

                return -1;
            }
        }

        // Feed the data into the digest builder
        m_digestBuilder->update(buf, rd);
        if (m_eof)
        {
            if (m_digestBuilder->finalise() != m_digest.value())
            {
                if (error)
                    *error = Error::format(ErrorCode::PackageSignatureInvalid, "File '%s' in archive failed to verify",
                                           m_path.c_str());
                return -1;
            }
        }
    }

    return rd;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Writes the contents of the archive entry to the given file descriptor.
    This also performs a verification of the file contents against the stored
    digest if there was one supplied at creation time.

    If any error with writing the file contents or if the digests don't match
    then \c false is returned and the error object is set.

*/
Result<size_t> ArchiveFileEntry::doFileWrite(int fd, const size_t maxSize,
                                             const std::unique_ptr<ILibarchiveReader> &archive)
{
    // Create a digest builder for verifying the file contents
    CryptoDigestBuilder digestBuilder(CryptoDigestBuilder::Algorithm::Sha256);

    const void *blockPtr = nullptr;
    int64_t blockOffset;
    size_t blockSize;
    int64_t currentOffset = 0;

    // Read the contents of the file and calculate the digest
    int rc;
    while ((rc = archive->readDataBlock(&blockPtr, &blockSize, &blockOffset)) == ARCHIVE_OK)
    {
        // Sanity check the entry is within the maximum size
        if ((blockSize > MAX_PACKAGE_ENTRY_SIZE) || ((blockOffset + blockSize) > MAX_PACKAGE_ENTRY_SIZE))
        {
            return Error(ErrorCode::PackageContentsInvalid, "Invalid size of entry in archive");
        }

        // Sanity check the offset hasn't gone backwards
        if (blockOffset < currentOffset)
        {
            return Error(ErrorCode::PackageContentsInvalid, "Invalid block offset in archive");
        }

        // Check we aren't exceeding the maximum allowed size
        if ((blockOffset + blockSize) > maxSize)
        {
            return Error::format(ErrorCode::PackageContentsInvalid, "File '%s' in archive exceeds maximum allowed size",
                                 m_path.c_str());
        }

        // Check if we need to add zero padding to the digest calculation
        if (blockOffset > currentOffset)
        {
            digestBuilder.update(nullptr, static_cast<size_t>(blockOffset - currentOffset));
        }

        // Update the digest with the block data
        digestBuilder.update(blockPtr, blockSize);

        // Write the data block to the output file
        if (TEMP_FAILURE_RETRY(pwrite(fd, blockPtr, blockSize, blockOffset)) != static_cast<ssize_t>(blockSize))
        {
            return Error(ErrorCode::PackageContentsInvalid, "Invalid block offset in archive");
        }

        currentOffset = blockOffset + static_cast<int64_t>(blockSize);
    }

    // Check we actually read the entire entry
    if (rc != ARCHIVE_EOF)
    {
        return Error(ErrorCode::PackageContentsInvalid, "Failed to read an entry in the archive");
    }

    // Finally check that the calculated digest matches the signed digest
    if (m_digest)
    {
        const auto actual = digestBuilder.finalise();
        if (m_digest.value() != actual)
            return Error(ErrorCode::PackageContentsInvalid, "Digest of entry in archive does not match signed digest");
    }

    // Check if the file is executable and set the mode if needed
    const mode_t mode = ((m_perms & std::filesystem::perms::owner_exec) == std::filesystem::perms::owner_exec) ? 0755
                                                                                                               : 0644;
    if (fchmod(fd, mode) != 0)
    {
        return Error::format(std::error_code(errno, std::system_category()), "Failed to set file mode for file '%s'",
                             m_path.c_str());
    }

    return Ok(static_cast<size_t>(currentOffset));
}

// -------------------------------------------------------------------------
/*!
    \internal

    Internal API to the library that is used by Package::extractTo().  It
    writes the contents of the archive entry to the given directory.

    This API shouldn't be used directly by clients of the library and shouldn't
    be used with read() at the same time.

 */
Result<size_t> ArchiveFileEntry::writeTo(int directoryFd, const size_t maxSize, Package::ExtractOptions options)
{
    Error error;

    // Checks if the parent reader object is still valid and still pointing to this entry
    auto readState = checkReadState(&error);
    if (!readState)
        return error;

    // Checks the target directory is valid and if the file already exists
    auto op = checkExtraction(directoryFd, m_path, std::filesystem::file_type::regular, m_perms, options);
    if (!op)
        return op.error();

    if (op.value() == ExtractOperation::Skip)
        return Ok(static_cast<size_t>(0));

    // Open the file for writing
    int fd = openat(directoryFd, m_path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0644);
    if (fd < 0)
    {
        return Error::format(std::error_code(errno, std::system_category()), "Failed to create file '%s'",
                             m_path.c_str());
    }

    // Write the file to the given file descriptor
    auto result = doFileWrite(fd, maxSize, readState->archive);
    if (result)
    {
        // Set the file permissions
        if (fchmod(fd, modeFromFsPerms(m_perms)) != 0)
        {
            result = Error::format(std::error_code(errno, std::system_category()),
                                   "Failed to set file mode for file '%s'", m_path.c_str());
        }

        // Set the file modification time
        struct timespec times[2];
        times[0].tv_sec = 0;
        times[0].tv_nsec = UTIME_OMIT;
        times[1].tv_sec = m_modTime;
        times[1].tv_nsec = 0;
        if (futimens(fd, times) != 0)
        {
            logSysWarning(errno, "Failed to set modification time for '%s'", m_path.c_str());
        }
    }

    if (!result)
    {
        // If failed to write or set the perms then remove the file
        if (unlinkat(directoryFd, m_path.c_str(), 0) != 0)
            logSysError(errno, "Failed to remove file '%s' after write failure", m_path.c_str());
    }

    // Close the written file
    if (close(fd) != 0)
        logSysError(errno, "Failed to close file '%s' after writing", m_path.c_str());

    return result;
}
