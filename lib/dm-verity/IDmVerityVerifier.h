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
        \interface IDmVerityVerifier
        \brief Object used to verify a block of data against a dm-verity hash tree.

        dm-verity is just a merkle hash tree, however the tree is stored in a file
        in such a way that the kernel can process the tree and use if for
        verification of block devices.

        Instances of this class understand the dm-verity hash tree layout, they
        use it to check a block from a given offset in the source file is valid,
        using the IDmVerityVerifier::verify method.

        You can think of this as a user-space version of the dm-verity kernel
        module.


        \see https://source.android.com/docs/security/features/verifiedboot/dm-verity
        \see https://docs.kernel.org/admin-guide/device-mapper/verity.html
        \see https://gitlab.com/cryptsetup/cryptsetup/-/wikis/DMVerity

     */
    class IDmVerityVerifier
    {
    public:
        // -------------------------------------------------------------------------
        /*!
            Builds a merkle hash tree from dm-verity formatted hash tree stored in
            the file at \a hashesFile.  The file is expected to have a valid
            dm-verity super block at the beginning of the file (or at hashesOffset
            if supplied).

            The \a rootHash must match the hash at the top of the hash tree stored
            in the file, if it doesn't then an error is reported and nullptr is
            returned.

            \a hashesOffset and \a hashesSize are optional, if not specified then
            it is assumed the offset is 0 and the size is the total size of
            \a hashesFile.   The offset and size parameters can be used if the hash
            tree is located within an existing file, which is the case for widgets
            where the hash tree is appended to the end of the file.

         */
        static LIBRALF_NS::Result<std::shared_ptr<IDmVerityVerifier>>
        fromHashesFile(const std::filesystem::path &hashesFile, const std::vector<uint8_t> &rootHash,
                       std::optional<size_t> hashesOffset = std::nullopt,
                       std::optional<size_t> hashesSize = std::nullopt);

        static LIBRALF_NS::Result<std::shared_ptr<IDmVerityVerifier>>
        fromHashesFile(int hashesFileFd, const std::vector<uint8_t> &rootHash,
                       std::optional<size_t> hashesOffset = std::nullopt,
                       std::optional<size_t> hashesSize = std::nullopt);

    public:
        virtual ~IDmVerityVerifier() = default;

        virtual size_t blockSize() const = 0;

        virtual size_t dataBlockCount() const = 0;

        virtual bool verify(size_t block, const void *data) const = 0;
    };

} // namespace entos::ralf::dmverity