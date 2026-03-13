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

#include "ErofsInodeFlatDataReader.h"

#include "core/LogMacros.h"

#include <cinttypes>

using namespace entos::ralf::erofs;

ErofsInodeFlatPlainDataReader::ErofsInodeFlatPlainDataReader(IErofsImageFile *file, size_t blockAddr, size_t blockSize,
                                                             size_t dataSize, unsigned advice)
    : m_file(file)
    , m_blocksAddress(blockAddr)
    , m_dataSize(dataSize)
    , m_accessAdvice(advice)
{
    (void)blockSize;

    if ((m_accessAdvice & ReadDataSequentially) && (m_dataSize > 8192))
    {
        // Ff we have more than a couple of uncompressed data blocks, then advise the kernel that we'll be reading
        // them sequentially, this allows it to optimise the read-ahead algo for us and we get a slight uplift in
        // performance
        m_file->advise(IErofsImageFile::Advice::Sequential, m_dataSize, m_blocksAddress);
    }
}

ErofsInodeFlatPlainDataReader::~ErofsInodeFlatPlainDataReader()
{
    if ((m_accessAdvice & ReadDataOnce) && (m_dataSize > 8192))
    {
        // If only reading the file once we can tell the kernel it can now remove this pages from the page cache,
        // this can stop other more important things from being paged out
        m_file->advise(IErofsImageFile::Advice::DontNeed, m_dataSize, m_blocksAddress);
    }
}

bool ErofsInodeFlatPlainDataReader::read(void *buf, size_t size, size_t offset)
{
    // Check not trying to read outside the data bounds
    if ((offset >= m_dataSize) || (size > (m_dataSize - offset)))
    {
        logError("plain data read is outside bounds");
        return false;
    }

    return m_file->read(buf, size, m_blocksAddress + offset);
}

ErofsInodeFlatInlineDataReader::ErofsInodeFlatInlineDataReader(IErofsImageFile *file, size_t blockAddr,
                                                               size_t tailBlockAddr, size_t blockSize, size_t dataSize,
                                                               unsigned advice)
    : m_file(file)
    , m_contigBlocksSize((dataSize / blockSize) * blockSize)
    , m_contigBlocksAddress(blockAddr)
    , m_tailBlockAddress(tailBlockAddr)
    , m_dataSize(dataSize)
    , m_accessAdvice(advice)
{
    if ((m_accessAdvice & ReadDataSequentially) && (m_contigBlocksSize >= 2))
    {
        // If we have more than a couple of uncompressed data blocks, then advise the kernel that we'll be reading them
        // sequentially, this allows it to optimise the read-ahead algo for us and we get a slight uplift in performance
        m_file->advise(IErofsImageFile::Advice::Sequential, m_contigBlocksSize, m_contigBlocksAddress);
    }
}

ErofsInodeFlatInlineDataReader::~ErofsInodeFlatInlineDataReader()
{
    if ((m_accessAdvice & ReadDataOnce) && (m_contigBlocksSize >= 2))
    {
        // If only reading the file once we can tell the kernel it can now remove this pages from the page cache,
        // this can stop other more important things from being paged out
        m_file->advise(IErofsImageFile::Advice::DontNeed, m_contigBlocksSize, m_contigBlocksAddress);
    }
}

bool ErofsInodeFlatInlineDataReader::read(void *buf, size_t size, size_t offset)
{
    // Check not trying to read outside the data bounds
    if ((offset >= m_dataSize) || (size > (m_dataSize - offset)))
    {
        logError("flat inline data read is outside bounds");
        return false;
    }

    // Check if reading the tail part of the block
    if ((offset + size) >= m_contigBlocksSize)
    {
        // Check if wholly within the tail
        if (offset >= m_contigBlocksSize)
        {
            const size_t tailOffset = offset - m_contigBlocksSize;
            return m_file->read(buf, size, m_tailBlockAddress + tailOffset);
        }
        else
        {
            // Need to read some of the data from the contiguous blocks and the rest from the tail

            // Calculate the number of bytes in the contiguous block (i.e. not in the tail)
            const size_t contigSize = m_contigBlocksSize - offset;

            // Read the data from the contiguous blocks
            if (!m_file->read(buf, contigSize, m_contigBlocksAddress + offset))
                return false;

            // Read the data from the tail
            void *tailBuf = reinterpret_cast<uint8_t *>(buf) + contigSize;
            return m_file->read(tailBuf, size - contigSize, m_tailBlockAddress);
        }
    }
    else
    {
        // All data is in the contiguous blocks, so just do a normal read
        return m_file->read(buf, size, m_contigBlocksAddress + offset);
    }
}
