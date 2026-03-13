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

#include "ErofsExtentMap.h"
#include "ErofsSuperBlock.h"
#include "IErofsImageFile.h"
#include "IErofsInodeDataReader.h"

#include <cstdint>

namespace entos::ralf::erofs
{

    // -----------------------------------------------------------------------------
    /*!
        \class ErofsInodeCompressedDataReader
        \brief Implements the data reader for the two types of compressed data.

        TODO: add full explanation on the format(s)

     */
    class ErofsInodeCompressedDataReader final : public IErofsInodeDataReader
    {
    public:
        static std::unique_ptr<ErofsInodeCompressedDataReader> createFull(IErofsImageFile *file,
                                                                          const ErofsSuperBlock *superBlock,
                                                                          size_t compressedBlocks, size_t extentsOffset,
                                                                          size_t dataSize, unsigned advice = 0);

        static std::unique_ptr<ErofsInodeCompressedDataReader>
        createCompact(IErofsImageFile *file, const ErofsSuperBlock *superBlock, size_t compressedBlocks,
                      size_t extentsOffset, size_t dataSize, unsigned advice = 0);

        ~ErofsInodeCompressedDataReader() final;

        bool read(void *buf, size_t size, size_t offset) override;

    private:
        ErofsInodeCompressedDataReader(IErofsImageFile *file, const ErofsSuperBlock *superBlock,
                                       std::unique_ptr<ErofsExtentMap> &&extents, size_t uncompressedSize,
                                       unsigned advice);

        bool readUncompressed(const ErofsExtentMap::Extent &extent, uint8_t *out, size_t offset, size_t size);
        bool readLz4Compressed(const ErofsExtentMap::Extent &extent, uint8_t *out, size_t offset, size_t size);

        bool decompressLz4(const std::vector<uint8_t> &block, uint8_t *out, size_t size) const;

        static std::pair<size_t, size_t> getExtendContigRange(const ErofsExtentMap *extentMap, size_t blockSize);

    private:
        IErofsImageFile *const m_file;

        /// Map that stores the start uncompressed index of the block against the actual physical block
        const std::unique_ptr<const ErofsExtentMap> m_extents;

        /// The size of the inode when uncompressed
        const size_t m_dataSize;

        /// The data block size
        const size_t m_blockSize;

        /// Data access advice flags
        const unsigned m_accessAdvice;

        /// Feature flag from superblock, determines if LZ4 data is padded at the beginning with zero bytes
        const bool m_haveLz4ZeroPadding;

        /// Buffer to store the blocks read from the backing file
        std::vector<uint8_t> m_blockBuffer;

        /// Stores the last decompressed block, we store this as if a client is reading through a file, the won't read the
        /// entire last block so we cache this so if on the next read we don't go a decompress the same buffer again
        size_t m_lastDecompressedBlockAddr = SIZE_MAX;
        std::vector<uint8_t> m_decompressedBlock;

        /// The start and end offsets of the compressed block range (if they are contiguous), this is used for
        /// posix_fadvice calls
        std::pair<size_t, size_t> m_contigBlockRange = { 0, 0 };
    };

} // namespace entos::ralf::erofs