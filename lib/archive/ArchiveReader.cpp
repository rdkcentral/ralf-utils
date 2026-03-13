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

#include "ArchiveReader.h"
#include "ArchiveEntries.h"
#include "ILibarchiveReader.h"
#include "core/Utils.h"

#include <archive.h>
#include <archive_entry.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

/// The maximum number of directories allowed in a archive; because the ArchiveReader builds a
/// tree of directories in memory, we need to limit the number of directories to prevent exploits.
#define MAX_DIRECTORY_COUNT (8192)

/// The maximum number of symlinks allowed in an archive; this is to prevent exploits that could
/// create a large number of symlinks that would consume memory and cause issues.
#define MAX_SYMLINK_COUNT (8192)

ArchiveReader::SharedReadState::SharedReadState(std::unique_ptr<ILibarchiveReader> &&archive)
    : archive(std::move(archive))
    , error()
    , endOfArchive(false)
    , entryIndex(0)
{
}

ArchiveReader::ArchiveReader(std::unique_ptr<ILibarchiveReader> &&archive, std::shared_ptr<const FileDigestMap> digests)
    : m_digests(std::move(digests))
    , m_fstreeSize(0)
    , m_maxArchiveDirectories(MAX_DIRECTORY_COUNT)
    , m_readState(std::make_shared<SharedReadState>(std::move(archive)))
    , m_verifiedEntries(0)
{
}

// -------------------------------------------------------------------------
/*!
    \internal

    Checks if the supplied \a parentPath has already been returned as a
    directory IPackageEntryImpl entry, if not then it is added to the stack.

    The PackageReader objects are required to return directory entries before
    any child entries, however libarchive doesn't guarantee this, so we have
    do this ourselves.

    Each new directory entry added will use the \a modTime as its modification
    time.
 */
void ArchiveReader::updateParentEntryStack(const std::filesystem::path &parentPath, const time_t dirModTime)
{
    // Iterate through the parent path, if come to a directory we haven't returned before then add it to the stack
    // of entries to return and update the file system tree
    FSTreeNode *current = &m_fstreeRoot;
    for (auto it = parentPath.begin(); it != parentPath.end(); ++it)
    {
        if (*it == "." || it->empty())
            continue;

        if (*it == "..")
        {
            setError(ErrorCode::PackageInvalidEntry, "Invalid path in archive entry");
            return;
        }

        const auto result = current->children.try_emplace(*it, FSTreeNode());
        if (result.second)
        {
            if (m_fstreeSize++ > m_maxArchiveDirectories)
            {
                setError(ErrorCode::PackageContentsInvalid, "Archive contains too many directories");
                return;
            }

            m_entryStack.push_back(ArchiveDirectoryEntry::create(parentPath.begin(), it, dirModTime));
        }

        current = &result.first->second;
    }
}

// -------------------------------------------------------------------------
/*!
    \internal

    Creates either a regular file or symlink entry object.  If an error occurs
    then nullptr is returned and the error object will be set.

 */
std::unique_ptr<IPackageEntryImpl> ArchiveReader::createEntry(std::filesystem::path &&path,
                                                              archive_entry *const archiveEntry)
{
    // Get the type of entry
    const mode_t type = (archive_entry_filetype(archiveEntry) & AE_IFMT);

    // Get the perms and modification time
    const mode_t perms = archive_entry_perm(archiveEntry);
    const time_t modTime = archive_entry_mtime(archiveEntry);

    // If we have file digest map, then verify that the supplied archive path has a match in the map, if
    // it doesn't it means the file is not signed and therefore the whole package is invalid.
    std::optional<std::vector<uint8_t>> digest;
    if (m_digests && ((type == AE_IFREG) || (type == AE_IFLNK)))
    {
        auto it = m_digests->find(path);
        if (it == m_digests->end())
        {
            setError(ErrorCode::PackageSignatureInvalid, "File '%s' in package not signed", path.c_str());
            return nullptr;
        }

        digest = it->second;
        m_verifiedEntries++;
    }

    // Create an entry based on type
    std::unique_ptr<IPackageEntryImpl> entry;
    switch (type)
    {
        case AE_IFLNK:
            // Create the symlink entry wrapper, this may return nullptr if an error occurs
            entry = ArchiveSymlinkEntry::create(m_readState, std::move(path), archive_entry_symlink_utf8(archiveEntry),
                                                modTime, digest);
            break;

        case AE_IFREG:
            // Create the regular file entry wrapper, this may return nullptr if an error occurs
            entry = ArchiveFileEntry::create(m_readState, std::move(path), archive_entry_size(archiveEntry),
                                             ((perms & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0), modTime, digest);
            break;

        default:
            setError(ErrorCode::PackageInvalidEntry, "Entry '%s' in archive is not a support type (0%o)", path.c_str(),
                     type);
            return nullptr;
    }

    // Check if failed to create entry reader, if so return an empty entry and the error reason should be set
    if (m_readState->error)
    {
        return nullptr;
    }

    return entry;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Returns true if the given \a path is within a symlink, i.e. the path
    starts with one of the symlinks in the archive.

    For security reasons we do not allow paths within symlinks in the
    archive to avoid files being extracted to arbitrary symlinked locations.

 */
bool ArchiveReader::withinSymLink(const std::filesystem::path &path) const
{
    for (const auto &symlink : m_symlinks)
    {
        auto result = std::mismatch(path.begin(), path.end(), symlink.begin(), symlink.end());
        if (result.second == symlink.end())
            return true;
    }

    return false;
}

// -------------------------------------------------------------------------
/*!
    Gets the next entry in the archive.  If an error occurs then nullptr
    is returned and the error object will be set.

    If reached the end of the archive then nullptr is also returned, but
    error should not be set and hasError() should return false.

 */
std::unique_ptr<IPackageEntryImpl> ArchiveReader::next()
{
    // Check if error is set, is so return an empty entry
    if (m_readState->error || m_readState->endOfArchive)
        return nullptr;

    // If the stack of queued entries is not empty then pop and return the next one
    if (!m_entryStack.empty())
    {
        std::unique_ptr<IPackageEntryImpl> entry = std::move(m_entryStack.front());
        m_entryStack.pop_front();
        return entry;
    }

    while (!m_readState->error)
    {
        // Otherwise get the next entry from the archive
        archive_entry *archiveEntry = nullptr;
        int ret = m_readState->archive->nextHeader(&archiveEntry);
        if (ret == ARCHIVE_EOF)
        {
            m_readState->endOfArchive = true;

            if (m_digests && (m_digests->size() != m_verifiedEntries))
                setError(ErrorCode::PackageSignatureInvalid, "Archive missing signed file(s)");

            return nullptr;
        }
        else if (!archiveEntry || (ret != ARCHIVE_OK))
        {
            setError(ErrorCode::PackageContentsInvalid, "Failure processing archive entry");
            return nullptr;
        }

        // Moved on to the next entry so update shared state, so if client held IPackageEntryImpl objects returned
        // previously, then they don't attempt to read from the archive (now we've moved on)
        m_readState->entryIndex++;

        // Get and check the path
        const char *archivePath = archive_entry_pathname(archiveEntry);
        if (!archivePath)
        {
            setError(ErrorCode::PackageInvalidEntry, "Null path in archive entry");
            return nullptr;
        }

        std::filesystem::path path = std::filesystem::path(archivePath).lexically_normal();
        if (!verifyPackagePath(path))
        {
            setError(ErrorCode::PackageInvalidEntry, "Invalid path '%s' in archive entry", path.c_str());
            return nullptr;
        }

        // Check that the path is not within a symlink, we are strict and don't allow this and treat it as an error
        if (withinSymLink(path))
        {
            setError(ErrorCode::PackageInvalidEntry, "Path '%s' in archive entry is within a symlink", path.c_str());
            return nullptr;
        }

        // Get the type of entry
        const mode_t type = (archive_entry_filetype(archiveEntry) & AE_IFMT);

        // Get the modification time
        const time_t modTime = archive_entry_mtime(archiveEntry);

        // If the entry is a directory then just build the parent entry stack to include the directory and we're done
        if (type == AE_IFDIR)
        {
            updateParentEntryStack(path, modTime);

            // If the entry stack is empty then it means we've already returned an entry for this directory, so get
            // the next entry from the archive
            if (m_entryStack.empty())
                continue;

            // Else return the directory entry on the stack
            std::unique_ptr<IPackageEntryImpl> entry = std::move(m_entryStack.front());
            m_entryStack.pop_front();
            return entry;
        }
        else
        {
            // Check that the path has a filename, if not then it's an invalid entry
            if (!path.has_filename())
            {
                setError(ErrorCode::PackageInvalidEntry,
                         "Invalid path '%s' in archive entry, non-directory entry must have a filename", path.c_str());
                return nullptr;
            }

            // "Readers" should guarantee that parent paths are iterated before children, however libarchive doesn't
            // do that, so we have to check the path of each entry, if we haven't returned a directory entry for the
            // parent then we do that now, before processing the actual entry
            if (path.has_parent_path())
            {
                updateParentEntryStack(path.parent_path(), modTime);
            }

            // If the entry is a symlink then store the symlink path so we can check against it later to make sure
            // no archive entries are within a symlink, this is a security measure
            if (type == AE_IFLNK)
            {
                if (m_symlinks.size() >= MAX_SYMLINK_COUNT)
                {
                    setError(ErrorCode::PackageContentsInvalid, "Archive contains too many symlinks");
                    return nullptr;
                }

                m_symlinks.emplace_back(path);
            }

            // Create the actual entry wrapper, this may return nullptr if an error occurs
            std::unique_ptr<IPackageEntryImpl> entry = createEntry(std::move(path), archiveEntry);
            if (!entry)
            {
                // Error should already be set
                return nullptr;
            }

            // If the stack of parent directories is not empty then we need to queue the entry and return the first
            // parent directory entry
            if (!m_entryStack.empty())
            {
                m_entryStack.push_back(std::move(entry));

                entry = std::move(m_entryStack.front());
                m_entryStack.pop_front();
            }

            // Return the entry
            return entry;
        }
    }

    return nullptr;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Sets the error state with the given error code and formatted message.
 */
void ArchiveReader::setError(LIBRALF_NS::ErrorCode code, const char *format, ...)
{
    std::va_list args;
    va_start(args, format);
    m_readState->error = Error::format(code, format, args);
    va_end(args);
}

// -------------------------------------------------------------------------
/*!
    Returns true if an error has occurred while reading the archive.

 */
bool ArchiveReader::hasError() const
{
    return m_readState->error.operator bool();
}

// -------------------------------------------------------------------------
/*!
    Returns the error object if an error has occurred while reading the archive.

 */
Error ArchiveReader::error() const
{
    return m_readState->error;
}
