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

#include "ErofsTypes.h"
#include "IErofsImageFile.h"

#include <sys/types.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace entos::ralf::erofs
{

    // -----------------------------------------------------------------------------
    /*!
        \class ErofsExtentMap
        \brief Stores a mapping of an uncompressed file range to a compressed block.

        EROFS uses fixed size compression blocks (4k in size), and obviously that
        fixed size compression block corresponds to a range in the uncompressed file.
        EROFS documentation calls the uncompressed range lcusters, and they
        correspond to pclusters (compressed blocks).

        There are two ways the mapping is recorded in EROFS, there is the legacy
        format which is relatively easy to understand and consists of an array of
        fixed size structures.  Each structure describes how you'd map an
        uncompressed file offset to a compressed fixed size block.

        The second format is the same in concept, however rather than an array
        of fixed size structures, there are compacted structures that may be 2-byte
        or 4-bytes in size, refer to fromCompactIndexes() for more details.


        Once the map is created, then you can use the find() method to get the
        Extent (compressed block) for a given uncompressed offset. You can then
        iterate through the Extents to read the compressed blocks and decompress
        them.

     */
    class ErofsExtentMap
    {
    public:
        enum class Compression : unsigned
        {
            None = 0,
            Lz4 = 1,
            Lzma = 2,
            Zip = 3,
            Zstd = 4,

            Invalid,
        };

        struct Extent
        {
            Compression compression;

            size_t uncomprOffset;  ///< The offset in the original file of the start of this block, i.e. if this was
                                   ///  0 then when the block is uncompressed it ia at the start of the file
            size_t uncomprSize;    ///< The uncompressed size of the extent
            size_t comprBlockAddr; ///< The offset within in the EROFS image file of the compressed block.  This
                                   ///  should be aligned on a block boundary. Each block has a fixed size matching
                                   ///  the EROFS image block size.
        };

        static std::unique_ptr<ErofsExtentMap> fromLegacyIndexes(IErofsImageFile *file, size_t extentsOffset,
                                                                 size_t compressedBlocks, size_t blockSize,
                                                                 size_t uncompressedSize);

        static std::unique_ptr<ErofsExtentMap> fromCompactIndexes(IErofsImageFile *file, size_t extentsOffset,
                                                                  size_t compressedBlocks, size_t blockSize,
                                                                  size_t uncompressedSize);

        ErofsExtentMap(size_t uncomprSized, size_t comprBlocks, size_t comprBlockSize,
                       ErofsCompressionAlgorithm comprAlgo0, ErofsCompressionAlgorithm comprAlgo1);

        ~ErofsExtentMap() = default;

        using const_iterator = std::vector<Extent>::const_iterator;

        const_iterator find(size_t offset) const;

        size_t size() const { return m_entries.size(); }

        inline const_iterator begin() const { return m_entries.begin(); }

        inline const_iterator end() const { return m_entries.end(); }

        void dump() const;

    private:
        bool add(ErofsLclusterType type, size_t uncomprOffset, size_t blockAddr);

        bool processCompact4BIndexes(const uint8_t *in, size_t custerIndex, size_t clusterSize);
        bool processSingleCompact4BIndex(const uint8_t *in, size_t custerIndex, size_t clusterSize);
        bool processCompact2BIndexes(const uint8_t *in, size_t custerIndex, size_t clusterSize);

        static Compression convertComprAlgo(ErofsCompressionAlgorithm algo);

    private:
        /// The fixed size of the compression block, typically 4096
        const size_t m_blockSize;

        /// The total uncompressed size of the file, this is used to calculate the uncompressed size of the last extent
        const size_t m_uncomprSize;

        /// The two possible compression algorithms used for the file. Erofs supports per block compression algo, so a
        /// file could have both lz4 and lzma compression blocks, the extent header determines which compression to use
        /// for each compressed block (that said I don't think mkfs.erofs supports dual compression at the moment)
        const Compression m_comprAlgos[2];

        /// This is the actual vector of extents, it's size is fixed at construction time
        std::vector<Extent> m_entries;

        size_t m_lastAddedIndex = SIZE_MAX;
    };

} // namespace entos::ralf::erofs