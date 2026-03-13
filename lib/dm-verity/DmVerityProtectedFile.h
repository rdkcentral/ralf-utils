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

#include "IDmVerityProtectedFile.h"
#include "IDmVerityVerifier.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace entos::ralf::dmverity
{

    // -----------------------------------------------------------------------------
    /*!
        \class DmVerityProtectedFile
        \brief Helper object that implements the IDmVerityProtectedFile interface,
        it verifies every block read from a backing file using an instance of
        IDmVerityVerifier.

        You'd typically create one of these object from a file containing both the
        data you're trying to protect and the dm-verity merkle hash tree.
        Internally this uses an instance of IDmVerityVerifier to verify every
        single block of data after it's been read from the file.

     */
    class DmVerityProtectedFile : public IDmVerityProtectedFile
    {
    public:
        DmVerityProtectedFile(std::shared_ptr<IDmVerityVerifier> verifier, int dataFileFd, size_t dataOffset,
                              size_t dataSize);
        ~DmVerityProtectedFile() override;

        size_t size() const override;

        bool read(void *buffer, size_t size, size_t offset) override;

    private:
        bool readDirectIo(void *buffer, size_t size, size_t offset);
        bool readAndVerifyBlock(size_t nblock, void *buffer) const;

        bool readPartialBlock(void *buffer, size_t nblock, size_t offset, size_t size);

    private:
        /// The block verifier
        const std::shared_ptr<IDmVerityVerifier> m_verifier;

        /// Details of the file to read
        const int m_fileFd;
        const size_t m_fileOffset;
        const size_t m_fileSize;

        /// Stores the dm-verity data block size
        const size_t m_blockSize;

        /// Used as a bounce buffer when reading a partial block
        uint8_t *m_blockBuffer = nullptr;

        /// The number of the block read into the mBlockBuffer
        size_t m_blockBufferNumber = SIZE_MAX;

        /// Set to true if the data file was opened with O_DIRECT, this changes how we read the file
        bool m_directIo = false;
    };

} // namespace entos::ralf::dmverity