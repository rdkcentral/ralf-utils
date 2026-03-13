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

#include "ErofsReader.h"
#include "ErofsRawImageFile.h"
#include "ErofsTypes.h"

#include "core/LogMacros.h"

#include <climits>
#include <cstdarg>
#include <cstring>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::erofs;

// -----------------------------------------------------------------------------
/*!
    Construct an ErofsEntry which is just a wrapper around a ErofsInode object
    with a path.

 */
ErofsEntry::ErofsEntry(std::shared_ptr<IErofsImageFile> backingFile, const ErofsSuperBlock *superBlock,
                       std::filesystem::path &&path, ino_t inode, unsigned inodeDataHints)
    : m_path(std::move(path))
    , m_inode(std::move(backingFile), superBlock, inode, inodeDataHints)
{
}

// -----------------------------------------------------------------------------
/*!
    Creates a EROFS image file reader for the image at \a imageFile.

    Will return a nullptr if there was any issue opening the file or checking
    the EROFS headers.  However this call doesn't do a full integrity check
    on the image, instead when walking the image you should check hasError().

 */
Result<std::shared_ptr<IErofsReader>> IErofsReader::create(const std::filesystem::path &imageFile)
{
    auto backingFile = ErofsRawImageFile::open(imageFile);
    if (!backingFile)
        return backingFile.error();

    auto reader = std::make_shared<ErofsReader>(std::move(backingFile.value()));
    if (reader->hasError())
        return reader->error();

    return reader;
}

// -----------------------------------------------------------------------------
/*!
    Creates a EROFS image file reader for the file wrapper \a imageFile.

    Typically \a imageFile will be an instance of DmVerityProtectedFile, so
    that the EROFS image is protected from tampering by a dm-verity hash tree.

    Will return a nullptr if there was any issue opening the file or checking
    the EROFS headers.  However this call doesn't do a full integrity check
    on the image, instead when walking the image you should check hasError().

 */
Result<std::shared_ptr<IErofsReader>> IErofsReader::create(std::shared_ptr<IErofsImageFile> imageFile)
{
    auto reader = std::make_shared<ErofsReader>(std::move(imageFile));
    if (reader->hasError())
        return reader->error();

    return reader;
}

// -----------------------------------------------------------------------------
/*!
    Constructs the EROFS reader for the image file, you should call isValid()
    after this to verify it ws successfully constructed.

 */
ErofsReader::ErofsReader(std::shared_ptr<IErofsImageFile> file)
    : m_file(std::move(file))
    , m_adviceFlags(Advice::None)
{
    // Read the first block (assuming 4kb blocks for now)
    uint8_t buf[4096];
    if (!m_file->read(buf, sizeof(buf), 0))
    {
        setError(ErrorCode::ErofsError, "Failed to read first block of EROFS image");
        return;
    }

    auto superBlock = ErofsSuperBlock::parse(buf, sizeof(buf));
    if (!superBlock)
    {
        setError(ErrorCode::ErofsError, "Failed to parse EROFS superblock - %s", superBlock.error().what());
        return;
    }

    // For now ensure that we only allow images with 4k block sizes
    if (superBlock->blockSize() != 4096)
    {
        logError("Don't support EROFS images with non-4k block sizes");
        return;
    }

    m_superBlock = superBlock.value();

    // Allocate a buffer to store directory blocks
    m_dirBlockBuffer.resize(superBlock->blockSize());

    // Read the root directory
    auto rootDirEntries = readDirectory("/", m_superBlock->rootInode());
    if (!rootDirEntries)
    {
        setError(ErrorCode::ErofsError, "Failed to read root directory entries");
        m_superBlock = std::nullopt;
        return;
    }

    m_currentDirIndex = 0;
    m_currentDir = std::move(rootDirEntries.value());

    // If root dir is not empty then push the root dir onto the stack
    if (!m_currentDir.empty())
        m_dirStack.push_back({ "", m_superBlock->rootInode() });
}

// -----------------------------------------------------------------------------
/*!
    Returns the current access advice flags set.

 */
IErofsReader::AdviceFlags ErofsReader::advice() const
{
    return m_adviceFlags;
}

// -----------------------------------------------------------------------------
/*!
    Sets the advice flags, this should be done before iterating through the
    files in the EROFS image.

 */
void ErofsReader::setAdvice(AdviceFlags advice)
{
    m_adviceFlags = advice;
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true if there was an error processing the file.  You should
    stop reading the fs image if this happens.

 */
bool ErofsReader::hasError() const
{
    return m_error.operator bool();
}

// -----------------------------------------------------------------------------
/*!
    The reason for the error.

 */
Error ErofsReader::error() const
{
    return m_error;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Sets the error string.

 */
void ErofsReader::setError(std::error_code code, const char *format, ...)
{
    std::va_list args;
    va_start(args, format);
    m_error = Error::format(code, format, args);
    va_end(args);
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true if haven't reached the end of the fs image.

 */
bool ErofsReader::hasNext() const
{
    return !m_error && !m_dirStack.empty();
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true when read all the file / directories from the image.

 */
bool ErofsReader::atEnd() const
{
    return m_dirStack.empty();
}

// -----------------------------------------------------------------------------
/*!
    Gets the current entry in the filesystem and then moves to the next entry.

 */
std::unique_ptr<IErofsEntry> ErofsReader::next()
{
    // Return invalid entry if we've reached the end
    if (m_dirStack.empty())
        return nullptr;

    // Or if encountered an error then stop reading more entries
    if (m_error)
        return nullptr;

    // Read the current entry
    const DirEntry &dirEntry = m_currentDir[m_currentDirIndex++];
    std::filesystem::path currentPath = m_dirStack.front().path / dirEntry.fileName;
    ino_t currentInode = dirEntry.inode;

    // If current entry is a directory then also push that onto the dir stack
    if (dirEntry.fileType == std::filesystem::file_type::directory)
        m_dirStack.push_back({ currentPath, currentInode });

    // Set any read hints on the inode
    unsigned inodeHints = 0;
    if (((m_adviceFlags & Advice::ReadAllOnce) == Advice::ReadAllOnce) &&
        (dirEntry.fileType == std::filesystem::file_type::regular))
    {
        // Tells the kernel to remove pages from page cache once read
        inodeHints |= IErofsInodeDataReader::ReadDataOnce;

        // Tells the kernel we'll be reading the blocks in a give range sequentially
        inodeHints |= IErofsInodeDataReader::ReadDataSequentially;
    }

    // Create an entry which wraps an inode in the fs image
    auto entry =
        std::make_unique<ErofsEntry>(m_file, &m_superBlock.value(), std::move(currentPath), currentInode, inodeHints);
    if (!entry || !entry->isValid())
    {
        setError(ErrorCode::ErofsError, "Error processing directory / file '%s/%s'", m_dirStack.front().path.c_str(),
                 dirEntry.fileName.c_str());
        return nullptr;
    }

    // Move to the next one
    if (m_currentDirIndex >= m_currentDir.size())
    {
        // clear current directory
        m_currentDir.clear();
        m_currentDirIndex = 0;

        // We're read everything from the current directory, so remove the directory from the stack, and go to the next
        // one
        m_dirStack.pop_front();

        // Find the next non-empty directory
        while (!m_dirStack.empty())
        {
            const Directory &front = m_dirStack.front();
            auto dirEntries = readDirectory(front.path, front.inode);
            if (!dirEntries)
            {
                setError(ErrorCode::ErofsError, "Error processing directory '%s'", front.path.c_str());
                return nullptr;
            }

            if (dirEntries->empty())
            {
                m_dirStack.pop_front();
            }
            else
            {
                m_currentDir = std::move(dirEntries.value());
                break;
            }
        }
    }

    return entry;
}

// -----------------------------------------------------------------------------
/*!
    Reads a directory inode and returns all the entries within the directory.
    Each entry simply consists of a file name, type and inode number.

 */
std::optional<std::vector<ErofsReader::DirEntry>> ErofsReader::readDirectory(const std::filesystem::path &dirPath,
                                                                             ino_t dirInode)
{
    ErofsInode inode(m_file, &m_superBlock.value(), dirInode);

    if (inode.type() != std::filesystem::file_type::directory)
    {
        logError("directory '%s' (inode %" PRIu64 ") is unexpectedly not an actual directory", dirPath.c_str(),
                 static_cast<uint64_t>(dirInode));
        return std::nullopt;
    }

    std::vector<ErofsReader::DirEntry> results;

    // Directory listing are divided into "directory blocks" of size 4k, each block is divided into two variable size
    // parts, "direntries" and "filenames"

    size_t offset = 0;
    const size_t totalSize = inode.size();
    const size_t blockSize = m_superBlock->blockSize();
    while (offset < totalSize)
    {
        // Read a block
        const size_t amount = std::min(blockSize, totalSize - offset);
        if (amount < sizeof(ErofsDirEntry))
        {
            logError("invalid sized directory block '%s' (inode %" PRIu64 ", size %zu)", dirPath.c_str(),
                     static_cast<uint64_t>(dirInode), totalSize);
            return std::nullopt;
        }

        if (!inode.read(m_dirBlockBuffer.data(), amount, offset, nullptr))
        {
            logError("failed to read directory block '%s' (inode %" PRIu64 ", offset %zu)", dirPath.c_str(),
                     static_cast<uint64_t>(dirInode), offset);
            return std::nullopt;
        }

        // Process the directory block
        if (!processDirectoryBlock(m_dirBlockBuffer.data(), amount, &results))
        {
            // error already logged
            return std::nullopt;
        }

        offset += amount;
    }

    return results;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes a single directory block, each block starts with one or more
    erofs_dirent structures, each structure contains an offset within the block
    of the file name.

    \see https://erofs.docs.kernel.org/en/latest/core_ondisk.html
 */
bool ErofsReader::processDirectoryBlock(const uint8_t *data, size_t dataSize, std::vector<ErofsReader::DirEntry> *entries)
{
    const auto *dirents = reinterpret_cast<const ErofsDirEntry *>(data);
    if ((dirents[0].nameOffset < sizeof(ErofsDirEntry)) || (dirents[0].nameOffset >= dataSize))
    {
        logError("directory block has invalid initial name offset");
        return false;
    }

    // The nameoff of the 1st directory entry also indicates the total number of directory entries in this directory
    // block
    const unsigned nEntries = dirents[0].nameOffset / sizeof(ErofsDirEntry);

    // Reserve space for the new entries
    entries->reserve(entries->size() + nEntries);

    // Read all the file entries
    for (unsigned i = 0; i < nEntries; i++)
    {
        // Sanity check the inode and type
        if ((dirents[i].nid > UINT32_MAX) || (dirents[i].fileType == static_cast<uint8_t>(ErofsFileType::Unknown)) ||
            (dirents[i].fileType >= static_cast<uint8_t>(ErofsFileType::Max)))
        {
            logError("invalid dirent, inode or file type is bogus");
            return false;
        }

        //
        const size_t nameOff = dirents[i].nameOffset;
        const char *name = reinterpret_cast<const char *>(data + nameOff);

        // Get the length of the filename
        size_t nameLen;
        if ((i + 1) == nEntries)
            nameLen = strnlen(name, dataSize - nameOff);
        else
            nameLen = dirents[i + 1].nameOffset - nameOff;

        // Sanity checks on the name length
        if ((nameLen == 0) || (nameLen > EROFS_MAX_NAME_LEN) || ((nameOff + nameLen) > dataSize))
        {
            logError("invalid dirent name offset and / or name length");
            return false;
        }

        // Skip dot and dot dot files
        if (((nameLen == 1) && (name[0] == '.')) || ((nameLen == 2) && (name[0] == '.') && name[1] == '.'))
        {
            continue;
        }

        // Get the filename
        std::string fileName(name, nameLen);

        // Sanity check the file name
        if (!validateFileName(fileName))
        {
            logError("found invalid file name '%s'", fileName.c_str());
            return false;
        }

        // Convert the type
        std::filesystem::file_type fileType;
        switch (static_cast<ErofsFileType>(dirents[i].fileType))
        {
            case ErofsFileType::RegularFile:
                fileType = std::filesystem::file_type::regular;
                break;
            case ErofsFileType::Directory:
                fileType = std::filesystem::file_type::directory;
                break;
            case ErofsFileType::Symlink:
                fileType = std::filesystem::file_type::symlink;
                break;
            case ErofsFileType::CharDevice:
                fileType = std::filesystem::file_type::character;
                break;
            case ErofsFileType::BlockDevice:
                fileType = std::filesystem::file_type::block;
                break;
            case ErofsFileType::Fifo:
                fileType = std::filesystem::file_type::fifo;
                break;
            case ErofsFileType::Socket:
                fileType = std::filesystem::file_type::socket;
                break;
            default:
                fileType = std::filesystem::file_type::unknown;
                break;
        }

        // Store the directory entry
        entries->emplace_back(DirEntry{ static_cast<ino_t>(dirents[i].nid), std::move(fileName), fileType });
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Verifies that the supplied string is a valid filename, it checks that:
        - is not an empty string
        - is not "." or ".."
        - doesn't contain any slashes ('/')
        - doesn't contain null characters ('\0')
        - is not longer than NAME_MAX

 */
bool ErofsReader::validateFileName(const std::string &fileName)
{
    if (fileName.empty() || (fileName.length() > NAME_MAX) || (fileName == ".") || (fileName == ".."))
        return false;

    for (const char &ch : fileName)
    {
        if ((ch == '/') || (ch == '\0'))
            return false;
    }

    return true;
}
