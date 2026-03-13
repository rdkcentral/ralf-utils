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

#include "ErofsInodeCompressedDataReader.h"

#include "core/LogMacros.h"

#include <lz4.h>

#include <cstring>
#include <vector>

using namespace entos::ralf::erofs;

std::unique_ptr<ErofsInodeCompressedDataReader>
ErofsInodeCompressedDataReader::createFull(IErofsImageFile *file, const ErofsSuperBlock *superBlock,
                                           size_t compressedBlocks, size_t extentsOffset, size_t dataSize,
                                           unsigned advice)
{
    auto extentMap =
        ErofsExtentMap::fromLegacyIndexes(file, extentsOffset, compressedBlocks, superBlock->blockSize(), dataSize);
    if (!extentMap)
        return nullptr;

    // extentMap->dump();

    return std::unique_ptr<ErofsInodeCompressedDataReader>(
        new ErofsInodeCompressedDataReader(file, superBlock, std::move(extentMap), dataSize, advice));
}

std::unique_ptr<ErofsInodeCompressedDataReader>
ErofsInodeCompressedDataReader::createCompact(IErofsImageFile *file, const ErofsSuperBlock *superBlock,
                                              size_t compressedBlocks, size_t extentsOffset, size_t dataSize,
                                              unsigned advice)
{
    auto extentMap =
        ErofsExtentMap::fromCompactIndexes(file, extentsOffset, compressedBlocks, superBlock->blockSize(), dataSize);
    if (!extentMap)
        return nullptr;

    // extentMap->dump();

    return std::unique_ptr<ErofsInodeCompressedDataReader>(
        new ErofsInodeCompressedDataReader(file, superBlock, std::move(extentMap), dataSize, advice));
}

ErofsInodeCompressedDataReader::ErofsInodeCompressedDataReader(IErofsImageFile *file, const ErofsSuperBlock *superBlock,
                                                               std::unique_ptr<ErofsExtentMap> &&extents,
                                                               size_t uncompressedSize, unsigned advice)
    : m_file(file)
    , m_extents(std::move(extents))
    , m_dataSize(uncompressedSize)
    , m_blockSize(superBlock->blockSize())
    , m_accessAdvice(advice)
    , m_haveLz4ZeroPadding(superBlock->hasFeature(ErofsSuperBlock::Feature::ZeroPadding))
{

    if (m_accessAdvice != 0)
    {
        m_contigBlockRange = getExtendContigRange(m_extents.get(), m_blockSize);
    }

    // There is a roughly 5% improvement in extraction performance for big files if we tell the kernel we're going to
    // read the compressed data blocks in order, this allows it to use read-ahead to fetch the flash blocks prior
    // to us needing them.
    if (m_accessAdvice & DataAccessAdvice::ReadDataSequentially)
    {
        if (m_contigBlockRange.second > m_blockSize)
        {
            m_file->advise(IErofsImageFile::Advice::Sequential, m_contigBlockRange.second, m_contigBlockRange.first);
        }
    }
}

ErofsInodeCompressedDataReader::~ErofsInodeCompressedDataReader()
{
    // The compressed blocks are typically stored sequentially in the file, although this is not guaranteed, however
    // we're going to assume the blocks are in sequence, and we'll eject all from the page cache.
    if (m_accessAdvice & DataAccessAdvice::ReadDataOnce)
    {
        if (m_contigBlockRange.second > m_blockSize)
        {
            m_file->advise(IErofsImageFile::Advice::DontNeed, m_contigBlockRange.second, m_contigBlockRange.first);
        }
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Gets the contiguous range of compressed blocks that the \a extentMap covers.
    If the compressed blocks are not contiguous then an empty range is returned.

    erofs-utils tools will create images with all the compressed blocks in order,
    however this is not guaranteed by the file system data structures, so this
    check compressed blocks are contiguous and if so returns the offset and
    size in bytes of the compressed blocks.

 */
std::pair<size_t, size_t> ErofsInodeCompressedDataReader::getExtendContigRange(const ErofsExtentMap *extentMap,
                                                                               size_t blockSize)
{
    auto it = extentMap->begin();
    size_t firstBlockAddr = it->comprBlockAddr;
    size_t prevBlockAddr = firstBlockAddr;
    ++it;

    for (; it != extentMap->end(); ++it)
    {
        size_t blockAddr = it->comprBlockAddr;
        if (blockAddr != (prevBlockAddr + blockSize))
            return { 0, 0 };

        prevBlockAddr = blockAddr;
    }

    return { firstBlockAddr, (prevBlockAddr - firstBlockAddr) + blockSize };
}

// -----------------------------------------------------------------------------
/*!
    Reads data from the compressed inode.  The file range between \a offset and
    \a size must be within the uncompressed data part of the inode or the
    function fails.

 */
bool ErofsInodeCompressedDataReader::read(void *buf, size_t size, size_t offset)
{
    // Sanity check, the entire read buffer must fit in the data
    if ((offset >= m_dataSize) || (size > (m_dataSize - offset)))
    {
        logError("compressed data read is outside bounds");
        return false;
    }

    auto *out = reinterpret_cast<uint8_t *>(buf);

    // Find the first compressed block that covers the data
    auto extent = m_extents->find(offset);
    if (extent == m_extents->end())
    {
        logError("failed to lookup offset %zu in extent map (this shouldn't happen)", offset);
        return false;
    }

    // Calculate the offset within to read from the first decompressed extent / cluster
    logDebug("reading m_extents -> { %zu, %zu, %zu }  (offset: %zu)", extent->uncomprOffset, extent->uncomprSize,
             extent->comprBlockAddr, offset);

    size_t clusterOffset = offset - extent->uncomprOffset;

    // Loop through the blocks until we either come to the end or requested data is not within the block
    while (size > 0)
    {
        // Calculate the decompressed amount to read
        const size_t clusterAmount = std::min(extent->uncomprSize - clusterOffset, size);

        switch (extent->compression)
        {
            case ErofsExtentMap::Compression::None:
                if (!readUncompressed(*extent, out, clusterOffset, clusterAmount))
                    return false;
                break;

            case ErofsExtentMap::Compression::Lz4:
                if (!readLz4Compressed(*extent, out, clusterOffset, clusterAmount))
                    return false;
                break;

            case ErofsExtentMap::Compression::Lzma:
                logError("LZMA compression not currently supported");
                return false;

            case ErofsExtentMap::Compression::Zip:
                logError("zip (deflate) compression not currently supported");
                return false;

            case ErofsExtentMap::Compression::Zstd:
                logError("zstd compression not currently supported");
                return false;

            default:
                return false;
        }

        // Move to the next extent / cluster
        out += clusterAmount;
        size -= clusterAmount;

        //
        clusterOffset = 0;

        // Move to the next block if not at the end
        if (size > 0)
            ++extent;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads an extent block and copies the data into the given \a out buffer.
    \a offset is the offset within the block to read from and \a size is the number
    of bytes to copy into the \a out buffer.

 */
bool ErofsInodeCompressedDataReader::readUncompressed(const ErofsExtentMap::Extent &extent, uint8_t *out, size_t offset,
                                                      size_t size)
{
    logDebug("reading uncompressed block @ %zu", extent.comprBlockAddr);

    // Read data from the uncompressed block
    return m_file->read(out, size, extent.comprBlockAddr + offset);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads an LZ4 compressed extent block and copies the data into the given
    \a out buffer.  \a offset is the offset within the uncompressed data to copy
    from and \a size is the number of bytes to copy into the \a out buffer.


 */
bool ErofsInodeCompressedDataReader::readLz4Compressed(const ErofsExtentMap::Extent &extent, uint8_t *out,
                                                       size_t offset, size_t size)
{
    logDebug("reading lz4 compressed block @ %zu (offset=%zu size=%zu)", extent.comprBlockAddr, offset, size);

    // Check if we've already decompressed this block and have it stored (this will be the case if the last read
    // performed did a partial read of a block)
    if (m_lastDecompressedBlockAddr == extent.comprBlockAddr)
    {
        // Just copy the already decompressed data into the output buffer
        memcpy(out, m_decompressedBlock.data() + offset, size);
    }
    else
    {
        // Create the buffer to store the compressed block if we haven't already
        if (m_blockBuffer.size() != m_blockSize)
            m_blockBuffer.resize(m_blockSize);

        // Read the compressed block
        if (!m_file->read(m_blockBuffer.data(), m_blockBuffer.size(), extent.comprBlockAddr))
        {
            logError("failed to read compressed block of size %zu at offset %zu", m_blockBuffer.size(),
                     extent.comprBlockAddr);
            return false;
        }

        // Check if we can just decompress straight into output buffer
        if ((offset == 0) && (extent.uncomprSize == size))
        {
            if (!decompressLz4(m_blockBuffer, out, size))
            {
                logError("failed to decompress block at offset %zu", extent.comprBlockAddr);
                return false;
            }
        }
        else
        {
            logDebug("decompressing into bounce buffer (extent.uncomprSize = %zu)", extent.uncomprSize);

            // Otherwise need to perform full decompress into a staging buffer first and then copy from that, we cache
            // the staging buffer for subsequent reads
            m_lastDecompressedBlockAddr = SIZE_MAX;
            if (m_decompressedBlock.size() < extent.uncomprSize)
                m_decompressedBlock.resize(extent.uncomprSize);

            if (!decompressLz4(m_blockBuffer, m_decompressedBlock.data(), extent.uncomprSize))
            {
                logError("failed to decompress block at offset %zu", extent.comprBlockAddr);
                return false;
            }

            // Store the address of the decompressed buffer so could be re-used for the next read
            m_lastDecompressedBlockAddr = extent.comprBlockAddr;

            // Then copy ouf the staging buffer
            memcpy(out, m_decompressedBlock.data() + offset, size);
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Decompresses an entire block and writes the output into the buffer \a out.
    \a size is the size of the \a out buffer and it MUST match the LZ4 decompression
    size, if it doesn't this call fails.

 */
bool ErofsInodeCompressedDataReader::decompressLz4(const std::vector<uint8_t> &block, uint8_t *out, size_t size) const
{
    const uint8_t *in = block.data();
    size_t inputMargin = 0;

    if (m_haveLz4ZeroPadding)
    {
        // if zero padding is enabled then skip over initial zero bytes
        for (inputMargin = 0; inputMargin < block.size(); inputMargin++)
        {
            if (in[inputMargin] != 0x00)
                break;
        }

        if (inputMargin >= block.size())
        {
            logError("zero padding enabled, but entire block is zeros?");
            return false;
        }
    }

    logDebug("lz4 decompressing block inputMargin=%zu, size=%zu", inputMargin, size);

    auto *src = reinterpret_cast<const char *>(in + inputMargin);
    auto *dst = reinterpret_cast<char *>(out);
    auto compressedSize = static_cast<int>(block.size() - inputMargin);

    int ret;
    if (m_haveLz4ZeroPadding)
    {
        ret = LZ4_decompress_safe(src, dst, compressedSize, static_cast<int>(size));
    }
    else
    {
        ret = LZ4_decompress_safe_partial(src, dst, compressedSize, static_cast<int>(size), static_cast<int>(size));
    }

    if (ret != static_cast<int>(size))
    {
        logError("LZ4_decompress_safe%s(%p, %p, %d, %d) failed with %d", (m_haveLz4ZeroPadding ? "" : "_partial"), src,
                 dst, compressedSize, static_cast<int>(size), ret);
    }

    return (ret == static_cast<int>(size));
}
