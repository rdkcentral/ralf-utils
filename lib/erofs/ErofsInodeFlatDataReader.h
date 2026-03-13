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

#include "IErofsImageFile.h"
#include "IErofsInodeDataReader.h"

namespace entos::ralf::erofs
{

    class ErofsInodeFlatPlainDataReader final : public IErofsInodeDataReader
    {
    public:
        ErofsInodeFlatPlainDataReader(IErofsImageFile *file, size_t blockAddr, size_t blockSize, size_t dataSize,
                                      unsigned advice = 0);
        ~ErofsInodeFlatPlainDataReader() final;

        bool read(void *buf, size_t size, size_t offset) override;

    private:
        IErofsImageFile *const m_file;

        /// The file offset of the first block of data
        const size_t m_blocksAddress;

        /// The number of inode data bytes
        const size_t m_dataSize;

        /// Data access advice flags
        const unsigned m_accessAdvice;
    };

    class ErofsInodeFlatInlineDataReader final : public IErofsInodeDataReader
    {
    public:
        ErofsInodeFlatInlineDataReader(IErofsImageFile *file, size_t blockAddr, size_t tailBlockAddr, size_t blockSize,
                                       size_t dataSize, unsigned advice = 0);
        ~ErofsInodeFlatInlineDataReader() final;

        bool read(void *buf, size_t size, size_t offset) override;

    private:
        IErofsImageFile *const m_file;

        /// The number of bytes that are NOT part of the tail, ie. if a read to offset greater than or equal to this then
        /// need to read the tail
        const size_t m_contigBlocksSize;

        /// The file offset of the first contiguous block
        const size_t m_contigBlocksAddress;

        /// The file offset of the start of the tail block
        const size_t m_tailBlockAddress;

        /// The number of inode data bytes
        const size_t m_dataSize;

        /// data access advice flags
        const unsigned m_accessAdvice;
    };

} // namespace entos::ralf::erofs