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

#include "LibRalf.h"
#include "Result.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace entos::ralf::dmverity
{

    // -----------------------------------------------------------------------------
    /*!
        \interface IDmVerityProtectedFile
        \brief Helper object that implements the IErofsImageFile interface, it
        verifies every block read from a backing file using an instance of
        IDmVerityVerifier.

        You'd typically create one of these object from a file containing both the
        data you're trying to protect and the dm-verity merkle hash tree.
        Internally this uses an instance of IDmVerityVerifier to verify every
        single block of data after it's been read from the file.

     */
    class IDmVerityProtectedFile
    {
    public:
        // -------------------------------------------------------------------------
        /*!
            Creates a dm-verity protected file reader.  \a dataFile and \a hashesFile
            may be the same file, in which case the offset and size options should
            point to different parts of the file.

            The \a rootHash obviously must match the root hash in the dm-verity
            part of the file, if it doesn't nullptr is returned.

         */
        static LIBRALF_NS::Result<std::shared_ptr<IDmVerityProtectedFile>>
        open(const std::filesystem::path &dataFile, std::optional<size_t> dataFileOffset,
             std::optional<size_t> dataFileSize, const std::filesystem::path &hashesFile,
             std::optional<size_t> hashesFileOffset, std::optional<size_t> hashesFileSize,
             const std::vector<uint8_t> &rootHash);

        static LIBRALF_NS::Result<std::shared_ptr<IDmVerityProtectedFile>>
        open(int dataFileFd, std::optional<size_t> dataFileOffset, std::optional<size_t> dataFileSize, int hashesFileFd,
             std::optional<size_t> hashesFileOffset, std::optional<size_t> hashesFileSize,
             const std::vector<uint8_t> &rootHash);

    public:
        virtual ~IDmVerityProtectedFile() = default;

        // -------------------------------------------------------------------------
        /*!
            Returns the size of the protected data.

         */
        virtual size_t size() const = 0;

        // -------------------------------------------------------------------------
        /*!
            Performs a read of the given number of bytes at the given \a offset
            from the data, verifies against the hash tree and if valid copies it
            into buffer.

            Returns \c true only if \a size bytes were successfully read, verified
            and copied into the buffer.  This API doesn't allow partial reads, ie.
            if \a offset + \a size is greater than the actual data then \c false
            is returned.

         */
        virtual bool read(void *buffer, size_t size, size_t offset) = 0;
    };

} // namespace entos::ralf::dmverity