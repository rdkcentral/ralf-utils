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

#include "ErofsExtentMap.h"
#include "ErofsTypes.h"

#include "core/LogMacros.h"

#include <algorithm>
#include <cstdint>
#include <map>

using namespace entos::ralf::erofs;

static constexpr size_t kMaxUncompressedSize = 512 * 1024 * 1024; // 512 MiB

// clang-format off
/// Macros for dealing with stuff on block boundaries
#define ROUND_UP(n, s) ((((n) + (s) - 1) / (s)) * (s))
#define DIV_ROUND_UP(n, d) (((n) + ((d) - 1)) / (d))
// clang-format on

// -----------------------------------------------------------------------------
/*!
    Creates an extent map from an array of legacy indexes.

   TODO: add detailed description of the format


 */
std::unique_ptr<ErofsExtentMap> ErofsExtentMap::fromLegacyIndexes(IErofsImageFile *file, size_t extentsOffset,
                                                                  size_t compressedBlocks, size_t blockSize,
                                                                  size_t uncompressedSize)
{
    const size_t maxBlockAddr = file->size() / blockSize;

    // Sanity checks
    if ((compressedBlocks > maxBlockAddr) || (uncompressedSize > kMaxUncompressedSize))
    {
        logError("Inode is too big, cannot decompress");
        return nullptr;
    }

    // The offset must be 8 byte aligned
    if (extentsOffset & 0x3)
    {
        logError("Invalid extentsOffset (%zu) alignment", extentsOffset);
        return nullptr;
    }

    logDebug("Reading z_erofs_map_header from offset %zu", extentsOffset);
    logDebug("CompressedBlocks=%zu", compressedBlocks);

    // Read the compressed map header right after the inode (+ xattr)
    ErofsZMapHeader header = {};
    if (!file || !file->read(&header, sizeof(ErofsZMapHeader), extentsOffset))
    {
        return nullptr;
    }

    // Get the compression algorithm used for the inode, there can actually be two different compression types, and
    // individual clusters / extends can choose to use no compression or one or other of the compression algos
    //    const ExtentMap::Compression compressionAlgo[2] = {
    //        convertAlgorithm((header.h_algorithmtype >> 0) & 0xf),
    //        convertAlgorithm((header.h_algorithmtype >> 4) & 0xf)
    //    };

    const auto algo1 = static_cast<ErofsCompressionAlgorithm>((header.algorithmType >> 0) & 0xf);
    const auto algo2 = static_cast<ErofsCompressionAlgorithm>((header.algorithmType >> 4) & 0xf);
    logDebug("Compression algorithms: head1=%u head2=%u", static_cast<unsigned>(algo1), static_cast<unsigned>(algo2));

    if ((algo1 != ErofsCompressionAlgorithm::Lz4) || (algo2 != ErofsCompressionAlgorithm::Lz4))
    {
        logError("Unknown or unsupported compression format for inode (%u, %u)", static_cast<unsigned>(algo1),
                 static_cast<unsigned>(algo2));
        return nullptr;
    }

    // Get the logical cluster size
    const size_t clusterSize = blockSize * (1 << (header.clusterBits & 0x07));
    const size_t logicalClusters = DIV_ROUND_UP(uncompressedSize, clusterSize);
    // logDebug("clusterSize=%zu : logicalClusters=%zu", clusterSize, logicalClusters);

    // Read all the z_erofs_lcluster_index structures, there is fixed 8 bytes between the end of the z_erofs_map_header
    // header and the first lcuster index
    std::vector<ErofsLclusterIndex> clusterIndexes(logicalClusters);
    if (!file->read(clusterIndexes.data(), logicalClusters * sizeof(ErofsLclusterIndex),
                    extentsOffset + sizeof(ErofsZMapHeader) + sizeof(ErofsLclusterIndex)))
    {
        return nullptr;
    }

    // Build the map of logical clusters (uncompressed byte indexes) to physical compressed blocks
    auto extentMap = std::make_unique<ErofsExtentMap>(uncompressedSize, compressedBlocks, blockSize, algo1, algo2);
    if (!extentMap)
    {
        return nullptr;
    }

    size_t logicalClusterNum = 0;
    for (const ErofsLclusterIndex &cluster : clusterIndexes)
    {
        logDebug("di.di_advise=%u : di.di_clusterofs=%u : di.di_u.blkaddr=%u", cluster.advise, cluster.clusterOffset,
                 cluster.data.blkAddr);

        // For building our simple map of logical to physical compressed clusters we don't care about non-head entries
        const auto type = static_cast<ErofsLclusterType>(cluster.advise & 0x3);
        if (type != ErofsLclusterType::NonHead)
        {
            size_t uncomprOffset = ((logicalClusterNum * clusterSize) | cluster.clusterOffset);
            if (!extentMap->add(type, uncomprOffset, cluster.data.blkAddr))
            {
                return nullptr;
            }
        }

        logicalClusterNum++;
    }

    // Check we filled the extent map
    if (extentMap->m_lastAddedIndex != (compressedBlocks - 1))
    {
        logError("failed to fill extent map");
        return nullptr;
    }

    return extentMap;
}

// -----------------------------------------------------------------------------
/*!
    Creates an extent map from compacted indexes in the given \a file at the
    given \a extentsOffset offset.

    TODO: add detailed description of the format

 */
std::unique_ptr<ErofsExtentMap> ErofsExtentMap::fromCompactIndexes(IErofsImageFile *file, size_t extentsOffset,
                                                                   size_t compressedBlocks, size_t blockSize,
                                                                   size_t uncompressedSize)
{
    const size_t maxBlockAddr = file->size() / blockSize;

    // The logic in here currently assumes a 4k block size
    if (blockSize != 4096)
    {
        logError("unsupported block size %zu", blockSize);
        return nullptr;
    }

    // Sanity checks
    if ((compressedBlocks > maxBlockAddr) || (uncompressedSize > kMaxUncompressedSize))
    {
        logError("inode is too big, cannot decompress (compressedBlocks=%zu, maxBlockAddr=%zu)", compressedBlocks,
                 maxBlockAddr);
        return nullptr;
    }

    // The offset must be 8 byte aligned
    if (extentsOffset & 0x3)
    {
        logError("invalid extentsOffset (%zu) alignment", extentsOffset);
        return nullptr;
    }

    logDebug("reading z_erofs_map_header from offset %zu", extentsOffset);
    logDebug("compressedBlocks=%zu", compressedBlocks);

    // Read the compressed map header right after the inode (+ xattr)
    ErofsZMapHeader header = {};
    if (!file->read(&header, sizeof(ErofsZMapHeader), extentsOffset))
    {
        return nullptr;
    }

    const auto algo1 = static_cast<ErofsCompressionAlgorithm>((header.algorithmType >> 0) & 0xf);
    const auto algo2 = static_cast<ErofsCompressionAlgorithm>((header.algorithmType >> 4) & 0xf);
    logDebug("Compression algorithms: head1=%u head2=%u", static_cast<unsigned>(algo1), static_cast<unsigned>(algo2));

    // Sanity check that other non-supported compression types aren't being used
    if (algo1 != ErofsCompressionAlgorithm::Lz4 || algo2 != ErofsCompressionAlgorithm::Lz4)
    {
        logError("Unknown or unsupported compression format (%hhu, %hhu) for inode", static_cast<uint8_t>(algo1),
                 static_cast<uint8_t>(algo2));
        return nullptr;
    }

    // We only support the Z_EROFS_ADVISE_COMPACTED_2B bit, if any other bit is set then we cannot process this
    // inode's data
    if ((header.advise & ~ErofsZMapHeaderAdviseBits::Compacted2b) != 0)
    {
        logError("Cluster has unsupported advise bits (0x%04x)", header.advise);
        return nullptr;
    }

    // Get the logical cluster size
    const size_t clusterSize = blockSize * (1 << (header.clusterBits & 0x07));
    const size_t logicalClusters = DIV_ROUND_UP(uncompressedSize, clusterSize);
    logDebug("clusterSize=%zu : logicalClusters=%zu", clusterSize, logicalClusters);

    //
    size_t indexesOffset = extentsOffset + sizeof(ErofsZMapHeader);

    // If using 'compacted 2b' entries, then they need to be aligned on a 32-byte boundary, if not aligned then the
    // beginning and end and padded with 'compacted 4b' entries
    unsigned int compacted_4b_initial, compacted_4b_end;
    unsigned int compacted_2b;
    if (header.advise & ErofsZMapHeaderAdviseBits::Compacted2b)
    {
        if (clusterSize != 4096)
        {
            logError("Compact 2B is unsupported for lcluster size %zu", clusterSize);
            return nullptr;
        }

        compacted_4b_initial = (32 - indexesOffset % 32) / 4;
        if (compacted_4b_initial == (32 / 4))
            compacted_4b_initial = 0;

        if (compacted_4b_initial > logicalClusters)
        {
            compacted_4b_initial = compacted_2b = 0;
            compacted_4b_end = logicalClusters;
        }
        else
        {
            compacted_2b = (logicalClusters - compacted_4b_initial) & ~0xf;
            compacted_4b_end = logicalClusters - compacted_4b_initial - compacted_2b;
        }
    }
    else
    {
        compacted_2b = compacted_4b_initial = 0;
        compacted_4b_end = logicalClusters;
    }

    // Read the indexes from the backing file
    const size_t indexesSize = (compacted_4b_initial * 4) + (compacted_2b * 2) + (ROUND_UP(compacted_4b_end, 2) * 4);
    logDebug("indexesSize=%zu", indexesSize);

    std::vector<uint8_t> indexes(indexesSize);
    if (!file->read(indexes.data(), indexesSize, indexesOffset))
        return nullptr;

    // Build the map of logical clusters (uncompressed byte indexes) to physical compressed blocks
    auto extentMap = std::make_unique<ErofsExtentMap>(uncompressedSize, compressedBlocks, blockSize, algo1, algo2);
    if (!extentMap)
    {
        return nullptr;
    }

    size_t dataOffset = 0;
    unsigned int custerIndex = 0;

    // Process the initial 4b compacted entries
    while (compacted_4b_initial)
    {
        if (!extentMap->processCompact4BIndexes(indexes.data() + dataOffset, custerIndex, clusterSize))
            return nullptr;

        compacted_4b_initial -= 2;
        custerIndex += 2;
        dataOffset += 8;
    }

    // Process the compacted 2b entries
    while (compacted_2b)
    {
        if (!extentMap->processCompact2BIndexes(indexes.data() + dataOffset, custerIndex, clusterSize))
            return nullptr;

        compacted_2b -= 16;
        custerIndex += 16;
        dataOffset += 32;
    }

    // Generate compacted_4b_end
    while (compacted_4b_end > 1)
    {
        if (!extentMap->processCompact4BIndexes(indexes.data() + dataOffset, custerIndex, clusterSize))
            return nullptr;

        compacted_4b_end -= 2;
        custerIndex += 2;
        dataOffset += 8;
    }

    // Generate final compacted_4b_end if needed
    if (compacted_4b_end > 0)
    {
        if (!extentMap->processSingleCompact4BIndex(indexes.data() + dataOffset, custerIndex, clusterSize))
            return nullptr;
    }

    // Check we filled the extent map
    if (extentMap->m_lastAddedIndex != (compressedBlocks - 1))
    {
        logError("failed to fill extent map");
        return nullptr;
    }

    return extentMap;
}

// -----------------------------------------------------------------------------
/*!
    \internal

 */
ErofsExtentMap::ErofsExtentMap(size_t uncomprSized, size_t comprBlocks, size_t comprBlockSize,
                               ErofsCompressionAlgorithm comprAlgo0, ErofsCompressionAlgorithm comprAlgo1)
    : m_blockSize(comprBlockSize)
    , m_uncomprSize(uncomprSized)
    , m_comprAlgos{ convertComprAlgo(comprAlgo0), convertComprAlgo(comprAlgo1) }
    , m_entries(comprBlocks)
{
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Converts an EROFS 4-bit compression algorithm to one of our internal types.

 */
ErofsExtentMap::Compression ErofsExtentMap::convertComprAlgo(ErofsCompressionAlgorithm algo)
{
    // NOLINTNEXTLINE(hicpp-multiway-paths-covered)
    switch (algo)
    {
        case ErofsCompressionAlgorithm::Lz4:
            return Compression::Lz4;

#if 0
        // only currently support LZ4 compression
        case ErofsCompressionAlgorithm::Lzma:
            return Compression::Lzma;
        case ErofsCompressionAlgorithm::Deflate:
            return Compression::Zip;
        case ErofsCompressionAlgorithm::Zstd:
            return Compression::Zstd;
#endif

        default:
            return Compression::Invalid;
    }
}

// -----------------------------------------------------------------------------
/*!
    Finds the extent that corresponds to the uncompressed file \a offset.

 */
ErofsExtentMap::const_iterator ErofsExtentMap::find(size_t offset) const
{
    // clang-format off
    // Find the block addr corresponding to the uncompressed file offset
    auto it = std::lower_bound(m_entries.cbegin(), m_entries.cend(), offset,
                               [](const Extent &entry, size_t value) -> bool
                               {
                                   return ((entry.uncomprOffset + entry.uncomprSize) - 1) < value;
                               });
    // clang-format on

    if ((it != m_entries.end()) && (it->uncomprOffset > offset))
        return it - 1;
    else
        return it;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Adds the Extent entry to the map.  Extents are required to be added strictly
    in order, if not then an error is returned.

 */
bool ErofsExtentMap::add(ErofsLclusterType type, size_t uncomprOffset, size_t blockAddr)
{
    if (uncomprOffset > m_uncomprSize)
    {
        logError("invalid uncompressed offset");
        return false;
    }

    // When parsing compacted indexes the last entry just contains the size of the uncompressed data, so ignore those
    // entries, provided we've already filled the map
    if (uncomprOffset == m_uncomprSize)
    {
        return true;
    }

    if (m_lastAddedIndex == SIZE_MAX)
    {
        m_lastAddedIndex = 0;
    }
    else if (m_lastAddedIndex >= (m_entries.size() - 1))
    {
        logError("already added all extents, yet received another");
        return false;
    }
    else
    {
        m_entries[m_lastAddedIndex].uncomprSize = uncomprOffset - m_entries[m_lastAddedIndex].uncomprOffset;
        m_lastAddedIndex++;
    }

    switch (type)
    {
        case ErofsLclusterType::Plain:
            m_entries[m_lastAddedIndex].compression = Compression::None;
            break;
        case ErofsLclusterType::Head1:
            m_entries[m_lastAddedIndex].compression = m_comprAlgos[0];
            break;
        case ErofsLclusterType::Head2:
            if (m_comprAlgos[1] == Compression::Invalid)
            {
                logError("Unknown or unsupported second compression format for inode");
                return false;
            }
            m_entries[m_lastAddedIndex].compression = m_comprAlgos[1];
            break;
        case ErofsLclusterType::NonHead:
            // Shouldn't be called, we don't care about non-head entries
            return false;

        default:
            logError("Unknown or unsupported cluster type %hhu", static_cast<uint8_t>(type));
            return false;
    }

    m_entries[m_lastAddedIndex].comprBlockAddr = blockAddr * m_blockSize;
    m_entries[m_lastAddedIndex].uncomprOffset = uncomprOffset;

    if (m_lastAddedIndex == (m_entries.size() - 1))
        m_entries[m_lastAddedIndex].uncomprSize = m_uncomprSize - uncomprOffset;
    else
        m_entries[m_lastAddedIndex].uncomprSize = 0; // filled in when next extent is added

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Process 8 bytes of '4b' compressed entries.  This consists of 2 16-bit
    entries followed by the compressed block address of the previous head
    entry.

    The MSB 2-bits of each entry describes the type of the entry. The lower
    bits make up the offset within the uncompressed cluster.

 */
bool ErofsExtentMap::processCompact4BIndexes(const uint8_t *in, size_t custerIndex, size_t clusterSize)
{
    struct Compact4bIndexes
    {
        uint16_t entries[2];
        uint32_t blkaddr;
    } __attribute__((packed));

    const auto *entry = reinterpret_cast<const Compact4bIndexes *>(in);

    uint32_t blkaddr = entry->blkaddr + 1;

    for (unsigned i = 0; i < 2; i++)
    {
        const auto type = static_cast<ErofsLclusterType>((entry->entries[i] >> 12) & 0x3);
        const unsigned offset = (entry->entries[i] >> 0) & (clusterSize - 1);

        if (type != ErofsLclusterType::NonHead)
        {
            // Only care about head entries, as they tell us details on compressed block
            if (!add(type, ((custerIndex + i) * clusterSize) | offset, blkaddr))
                return false;

            blkaddr++;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    The same as processCompact4BIndexes() but ignores the second entry.

 */
bool ErofsExtentMap::processSingleCompact4BIndex(const uint8_t *in, size_t custerIndex, size_t clusterSize)
{
    struct Compact4bIndexes
    {
        uint16_t entries[2];
        uint32_t blkaddr;
    } __attribute__((packed));

    const auto *entry = reinterpret_cast<const Compact4bIndexes *>(in);

    uint32_t blkaddr = entry->blkaddr + 1;

    for (unsigned i = 0; i < 1; i++)
    {
        const auto type = static_cast<ErofsLclusterType>((entry->entries[i] >> 12) & 0x3);
        const unsigned offset = (entry->entries[i] >> 0) & (clusterSize - 1);

        if (type != ErofsLclusterType::NonHead)
        {
            if (!add(type, ((custerIndex + i) * clusterSize) | offset, blkaddr))
                return false;

            blkaddr++;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Process 32 bytes of compressed entries.  There are 16 entries stored in the
    32 bytes, each entry is 14bits and the last 32-bits of the data is the
    first compressed block address:

        +-------+-------+-------+      +---------------+
        | entry | entry | entry |  ... |   blkaddr     |
        +-------+-------+-------+      +---------------+
         14bits  14bits  14bits            32 bits

    The MSB 2-bits of each entry describes the type of the entry. The lower
    bits make up the offset within the uncompressed cluster.

 */
bool ErofsExtentMap::processCompact2BIndexes(const uint8_t *in, size_t custerIndex, size_t clusterSize)
{
    // First step is to read all the 14-bit entries
    unsigned entries[16];
    for (unsigned i = 0; i < 16; i++)
    {
        size_t bitOffset = (i * 14);
        size_t byteOffset = (bitOffset / 8);

        uint32_t v = *reinterpret_cast<const uint32_t *>(in + byteOffset);
        v = v >> (bitOffset % 8);

        // Store the 14-bit entry
        entries[i] = (v & 0x3fff);
    }

    // Get the 4-byte block address at the end
    uint32_t blkaddr = *reinterpret_cast<const uint32_t *>(in + 28);
    blkaddr += 1;

    // Go through all the entries in reverse order, any HEAD entries we add to the cluster map
    for (unsigned i = 0; i < 16; i++)
    {
        const auto type = static_cast<ErofsLclusterType>((entries[i] >> 12) & 0x3);
        const unsigned offset = (entries[i] >> 0) & (clusterSize - 1);

        if (type != ErofsLclusterType::NonHead)
        {
            if (!add(type, ((custerIndex + i) * clusterSize) | offset, blkaddr))
                return false;

            blkaddr++;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    Debugging function to dump out the complete extents map.

 */
void ErofsExtentMap::dump() const
{
    static const std::map<Compression, const char *> compressionNames = {
        { Compression::Invalid, "?err" }, { Compression::None, "none" }, { Compression::Lz4, "lz4" },
        { Compression::Zip, "zip" },      { Compression::Lzma, "lzma" }, { Compression::Zstd, "zstd" },
    };

    logInfo(" Ext:   logical offset   |  length :     physical offset    |  length  | compr");

    int n = 0;
    for (const Extent &extent : m_entries)
    {
        logInfo("%4d: %8zu..%8zu | %7zu : %10zu..%10zu | %8zu | %s", n++, extent.uncomprOffset,
                extent.uncomprOffset + extent.uncomprSize, extent.uncomprSize, extent.comprBlockAddr,
                extent.comprBlockAddr + m_blockSize, m_blockSize, compressionNames.at(extent.compression));
    }
}
