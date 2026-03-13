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

#include "ErofsInode.h"
#include "ErofsInodeCompressedDataReader.h"
#include "ErofsInodeFlatDataReader.h"

#include "core/LogMacros.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cinttypes>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::erofs;

ErofsInode::ErofsInode(std::shared_ptr<IErofsImageFile> file, const ErofsSuperBlock *superBlock, ino_t inode,
                       unsigned hints)
    : m_superBlock(superBlock)
    , m_backingFile(std::move(file))
    , m_inodeNumber(inode)
    , m_inodeFileOffset(m_superBlock->inodeOffset(inode))
    , m_inodeReaderHints(hints)
{
    // Read the inode, assume compact 32-byte inode to start with
    uint8_t buf[64];

    if (!m_backingFile->read(buf, 32, m_inodeFileOffset))
    {
        logError("failed to read 32-byte inode structure");
        return;
    }

    // Process the inode structure, if bit 0 of the format field is set then it's an extended inode, and we need to read
    // the second 32 bytes
    auto *compactInode = reinterpret_cast<const ErofsInodeCompact *>(buf);
    if ((compactInode->format & 0x1) == 0)
    {
        processCompactInode(compactInode);
    }
    else
    {
        // Read the second 32 bytes of an extended record
        if (!m_backingFile->read(buf + 32, 32, (m_inodeFileOffset + 32)))
        {
            logError("failed to read 64-byte inode structure");
            return;
        }

        auto *extendedInode = reinterpret_cast<const ErofsInodeExtended *>(buf);
        processExtendedInode(extendedInode);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Stores the data format of the inode based on the format field in the
    inode header.

 */
void ErofsInode::processDataFormat(const uint16_t inodeFormat)
{
    switch (static_cast<ErofsInodeFormat>((inodeFormat >> 1) & 0x7))
    {
        case ErofsInodeFormat::FlatPlain:
            m_dataFormat = DataFormat::FlatPlain;
            break;
        case ErofsInodeFormat::CompressedFull:
            m_dataFormat = DataFormat::CompressedFull;
            break;
        case ErofsInodeFormat::FlatInline:
            m_dataFormat = DataFormat::FlatInline;
            break;
        case ErofsInodeFormat::CompressedCompact:
            m_dataFormat = DataFormat::CompressedCompact;
            break;
        case ErofsInodeFormat::ChunkBased:
            m_dataFormat = DataFormat::ChunkBased;
            break;
        default:
            logWarning("Unknown or unsupported EROFS inode data format %u", (inodeFormat >> 1) & 0x7);
            m_dataFormat = DataFormat::Invalid;
            break;
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Stores the mode and sets the inode type from the mode field from the inode
    structure.

 */
void ErofsInode::processMode(mode_t mode)
{
    m_mode = mode;

    switch (mode & S_IFMT)
    {
        case S_IFDIR:
            m_type = std::filesystem::file_type::directory;
            break;
        case S_IFREG:
            m_type = std::filesystem::file_type::regular;
            break;
        case S_IFLNK:
            m_type = std::filesystem::file_type::symlink;
            break;

        default:
            logError("unsupported erofs inode type 0%06o", (mode & S_IFMT));
            return;
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Processes a 32-byte 'compact' inode header, storing it's details within
    this object.

 */
void ErofsInode::processCompactInode(const ErofsInodeCompact *compactInode)
{
    // TODO: add support for xattr
    if (compactInode->xattrCount != 0)
    {
        logError("don't support EROFS inodes with xattrs");
        return;
    }

    m_inodeHeaderSize = 32;
    m_size = compactInode->size;

    processDataFormat(compactInode->format);
    processMode(compactInode->mode);

    switch (m_dataFormat)
    {
        case DataFormat::FlatPlain:
        case DataFormat::FlatInline:
            if (compactInode->data.rawBlkAddr == INT32_MAX)
                m_rawBlockAddress = SIZE_MAX;
            else
                m_rawBlockAddress = compactInode->data.rawBlkAddr * m_superBlock->blockSize();
            break;
        case DataFormat::CompressedFull:
        case DataFormat::CompressedCompact:
            m_compressedBlocks = compactInode->data.compressedBlocks;
        default:
            break;
    }

    m_uid = compactInode->uid;
    m_gid = compactInode->gid;

    m_modificationTime = m_superBlock->buildTime();
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Processes a 64-byte 'extended' inode header, storing it's details within
    this object.

 */
void ErofsInode::processExtendedInode(const ErofsInodeExtended *extendedInode)
{
    // TODO: add support for xattr
    if (extendedInode->xattrCount != 0)
    {
        logError("don't support EROFS inodes with xattrs");
        return;
    }

    if (extendedInode->size >= SIZE_MAX)
    {
        logError("EROFS inode data to large (%" PRIu64 " bytes)", extendedInode->size);
        return;
    }

    m_inodeHeaderSize = 64;
    m_size = static_cast<size_t>(extendedInode->size);

    processDataFormat(extendedInode->format);
    processMode(extendedInode->mode);

    switch (m_dataFormat)
    {
        case DataFormat::FlatPlain:
        case DataFormat::FlatInline:
            if (extendedInode->data.rawBlkAddr == INT32_MAX)
                m_rawBlockAddress = SIZE_MAX;
            else
                m_rawBlockAddress = extendedInode->data.rawBlkAddr * m_superBlock->blockSize();
            break;
        case DataFormat::CompressedFull:
        case DataFormat::CompressedCompact:
            m_compressedBlocks = extendedInode->data.compressedBlocks;
            break;
        default:
            break;
    }

    m_uid = extendedInode->uid;
    m_gid = extendedInode->gid;

    m_modificationTime = { static_cast<time_t>(extendedInode->mtimeSecs), static_cast<long>(extendedInode->mtimeNSecs) };
}

// -----------------------------------------------------------------------------
/*!
    Returns the permissions set on the inode.

 */
std::filesystem::perms ErofsInode::permissions() const
{
    std::filesystem::perms perms = std::filesystem::perms::none;

    if (m_mode & S_IXOTH)
        perms |= std::filesystem::perms::others_exec;
    if (m_mode & S_IWOTH)
        perms |= std::filesystem::perms::others_write;
    if (m_mode & S_IROTH)
        perms |= std::filesystem::perms::others_read;

    if (m_mode & S_IXGRP)
        perms |= std::filesystem::perms::group_exec;
    if (m_mode & S_IWGRP)
        perms |= std::filesystem::perms::group_write;
    if (m_mode & S_IRGRP)
        perms |= std::filesystem::perms::group_read;

    if (m_mode & S_IXUSR)
        perms |= std::filesystem::perms::owner_exec;
    if (m_mode & S_IWUSR)
        perms |= std::filesystem::perms::owner_write;
    if (m_mode & S_IRUSR)
        perms |= std::filesystem::perms::owner_read;

    return perms;
}

// -----------------------------------------------------------------------------
/*!
    Performs a read on the inode.  The data in the inode depends on the inode
    type, for a regular file it will be the actual file data, if a directory
    then it will be a directory tree, etc.  This method makes no assumptions
    on the format of the actual data.

    Inode data can be stored in different ways:
        - plain uncompressed data
        - plain uncompressed data with tail packing
        - compressed data for flat lookup tables
        - compressed data with compacted lookup tables
        - chunked data (not currently supported)

    This method uses an implementation of the IErofsInodeDataReader that matches
    the format to read the actual data.
 */
ssize_t ErofsInode::read(void *buf, size_t size, size_t offset, Error *error) const
{
    if ((offset >= m_size) || (size == 0))
        return 0;

    if (size > (m_size - offset))
        size = (m_size - offset);

    // if we haven't yet created the reader object do that now, we do this
    // lazily rather than at construction time as the reader may skip
    // inode's it doesn't care about, ie. if just creating a directory
    // tree
    if (!m_inodeReader)
    {
        switch (m_dataFormat)
        {
            case DataFormat::FlatInline:
                m_inodeReader = std::make_unique<ErofsInodeFlatInlineDataReader>(m_backingFile.get(), m_rawBlockAddress,
                                                                                 m_inodeFileOffset + m_inodeHeaderSize,
                                                                                 m_superBlock->blockSize(), m_size,
                                                                                 m_inodeReaderHints);
                break;
            case DataFormat::FlatPlain:
                m_inodeReader = std::make_unique<ErofsInodeFlatPlainDataReader>(m_backingFile.get(), m_rawBlockAddress,
                                                                                m_superBlock->blockSize(), m_size,
                                                                                m_inodeReaderHints);
                break;

            case DataFormat::ChunkBased:
                logError("inode %" PRIu64 " is using chunked data which we do not support",
                         static_cast<uint64_t>(m_inodeNumber));
                return -1;

            case DataFormat::CompressedFull:
                m_inodeReader = ErofsInodeCompressedDataReader::createFull(m_backingFile.get(), m_superBlock,
                                                                           m_compressedBlocks,
                                                                           m_inodeFileOffset + m_inodeHeaderSize,
                                                                           m_size, m_inodeReaderHints);
                break;

            case DataFormat::CompressedCompact:
                m_inodeReader = ErofsInodeCompressedDataReader::createCompact(m_backingFile.get(), m_superBlock,
                                                                              m_compressedBlocks,
                                                                              m_inodeFileOffset + m_inodeHeaderSize,
                                                                              m_size, m_inodeReaderHints);
                break;

            default:
                logError("unknown inode data format for inode %" PRIu64, static_cast<uint64_t>(m_inodeNumber));
                if (error)
                {
                    *error = Error::format(ErrorCode::ErofsError, "Unknown inode data format for inode %" PRIu64,
                                           static_cast<uint64_t>(m_inodeNumber));
                }
                return -1;
        }

        if (!m_inodeReader)
        {
            logError("failed to create inode data reader for inode %" PRIu64, static_cast<uint64_t>(m_inodeNumber));
            if (error)
            {
                *error = Error::format(ErrorCode::ErofsError, "Failed to create inode data reader for inode %" PRIu64,
                                       static_cast<uint64_t>(m_inodeNumber));
            }

            return -1;
        }
    }

    if (m_inodeReader->read(buf, size, offset))
    {
        return static_cast<ssize_t>(size);
    }
    else
    {
        logError("reader failed to read (reader type %d)", static_cast<int>(m_dataFormat));
        if (error)
        {
            *error = Error::format(ErrorCode::ErofsError, "Reader failed to read (reader type %d)",
                                   static_cast<int>(m_dataFormat));
        }
        return -1;
    }
}

std::string ErofsInode::toString() const
{
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "Inode:\n"
             "\tsize:        %zu\n"
             "\tinode size:  %zu\n"
             "\tperms:       0%04o\n"
             "\tdata format: %d\n",
             m_size, m_inodeHeaderSize, (m_mode & 07777), int(m_dataFormat));

    return buf;
}
