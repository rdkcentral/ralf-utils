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

#pragma once

#include "ErofsSuperBlock.h"
#include "Error.h"
#include "IErofsImageFile.h"
#include "IErofsInodeDataReader.h"

#include <filesystem>
#include <memory>
#include <string>

#include <sys/types.h>

struct erofs_inode_compact;
struct erofs_inode_extended;

namespace entos::ralf::erofs
{

    // -----------------------------------------------------------------------------
    /*!
        \class ErofsInode
        \brief Wrapper around an inode entry in the EROFS image file.

        Inodes structures are for every entity in the FS image; directories, files,
        symlinks, char devices, etc.  In EROFS inodes have a 32 or 64 byte structure
        describing their details.

        The data for an inode (a regular file's contents, a directory's table, a
        symlink target, etc) can be stored in 5 possible formats, there is individual
        classes inherited from IErofsInodeDataReader to read the data.

        \see https://erofs.docs.kernel.org/en/latest/core_ondisk.html

     */
    class ErofsInode
    {
    public:
        enum Hint : unsigned
        {
            ExpectSequentialRead = (1 << 0), ///< This hint indicates that the data stored in the inode will be read
                                             ///  in full and sequentially.
        };

    public:
        ErofsInode(std::shared_ptr<IErofsImageFile> file, const ErofsSuperBlock *superBlock, ino_t inode,
                   unsigned hints = 0);
        ~ErofsInode() = default;

        bool isValid() const { return (m_type != std::filesystem::file_type::unknown); }

        inline size_t size() const { return m_size; }

        inline std::filesystem::file_type type() const { return m_type; }

        inline time_t modificationTime() const { return m_modificationTime.tv_sec; }

        std::filesystem::perms permissions() const;

        inline uid_t owner() const { return m_uid; }

        inline gid_t group() const { return m_gid; }

        ssize_t read(void *buf, size_t size, size_t offset, LIBRALF_NS::Error *error) const;

        std::string toString() const;

    private:
        void processCompactInode(const ErofsInodeCompact *compactInode);
        void processExtendedInode(const ErofsInodeExtended *extendedInode);

        enum class DataFormat : unsigned
        {
            FlatPlain = 0,
            CompressedFull,
            FlatInline,
            CompressedCompact,
            ChunkBased,

            Invalid,
        };

        void processDataFormat(uint16_t inodeFormat);
        void processMode(mode_t mode);

    private:
        /// The superblock for the erofs image
        const ErofsSuperBlock *const m_superBlock;

        /// The backing erofs image file object
        const std::shared_ptr<IErofsImageFile> m_backingFile;

        /// Store just for debugging / error messages
        const ino_t m_inodeNumber;

        /// The offset (in bytes) of the inode structure in the backing file
        const size_t m_inodeFileOffset;

        /// Hints for how the inode is expected to be accessed
        const unsigned m_inodeReaderHints;

        /// The size in bytes of the inode header, should also take into account xattr data, such that
        /// m_inodeFileOffset + m_inodeHeaderSize equals the file offset immediately after the inode header
        size_t m_inodeHeaderSize = 0;

        /// The data format of the inode
        DataFormat m_dataFormat = DataFormat::Invalid;

        /// The type of entity represented by the inode
        std::filesystem::file_type m_type = std::filesystem::file_type::unknown;

        /// The size of the inode's data; for regular files this is the uncompressed file size, for symlinks this is the
        /// size of (in bytes) of the target string and for directories it is the size of the dirent table of directory
        /// entries.
        size_t m_size = 0;

        /// The access bits of the file or directory
        mode_t m_mode = 0;

        /// The owner and group of the file or directory
        uid_t m_uid = 0;
        gid_t m_gid = 0;

        /// The modification time of the file or directory
        timespec m_modificationTime = { 0, 0 };

        /// Block address for uncompressed flat inodes, only used if m_dataFormat is FlatPlain or FlatInline
        size_t m_rawBlockAddress = 0;

        /// If the data format for the inode indicates it is compressed, this is the total number of compress blocks
        /// (see ErofsEntityMap)
        size_t m_compressedBlocks = 0;

        /// The data reader object, there is a different "data reader" for each of the 5 possible inode data formats
        /// (see DataFormat)
        mutable std::unique_ptr<IErofsInodeDataReader> m_inodeReader;
    };

} // namespace entos::ralf::erofs