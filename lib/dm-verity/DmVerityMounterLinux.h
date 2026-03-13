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

#include "EnumFlags.h"
#include "IDmVerityMounter.h"
#include "LibRalf.h"
#include "Result.h"
#include "core/IPackageMountImpl.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace entos::ralf::dmverity
{

    class DmVerityMounterLinux : public IDmVerityMounter
    {
    public:
        DmVerityMounterLinux() = default;
        ~DmVerityMounterLinux() override = default;

        // -------------------------------------------------------------------------
        /*!
            Creates a dm-verity protected loopback mount of the given file.

            \a name is typically the package id and will form part of the devmapper
            volume name and uuid, it is not required to be unique as the code will
            add a random string to both the volume name and uuid.
            \a imagePath is the path to the image file to loopback mount, this file
            should contain the actual fs image in \a dataRange and the dm-verity
            hash tree in \a hashesRange.  The \a hashesRange should contain the
            standard dm-verity superblock.  The kernel expects the hashes to be
            located after the data in the image file, if not then separate loopback
            devices have to be created for the hashes and data which is inefficient
            (and not currently supported by this code).

            \a mountPath is the directory to mount to, the path must be an existing
            directory or the function fails.

            \a rootHash is the verified dm-verity root hash, if this doesn't match
            the hashes in the \a imagePath file then the mount fails.

            The devmapper and loopback devices are created and then released after
            mounting, this ensures that once unmounted everything is cleaned up and
            no further action is required to either remove the devmapper device
            or the loop device.

         */
        LIBRALF_NS::Result<std::unique_ptr<LIBRALF_NS::IPackageMountImpl>>
        mount(std::string_view name, FileSystemType fsType, const std::filesystem::path &imagePath,
              const std::filesystem::path &mountPoint, FileRange dataRange, FileRange hashesRange,
              const std::vector<uint8_t> &rootHash, const std::vector<uint8_t> &salt,
              LIBRALF_NS::MountFlags flags) const override;

        LIBRALF_NS::Result<std::unique_ptr<LIBRALF_NS::IPackageMountImpl>>
        mount(std::string_view name, FileSystemType fsType, int imageFd, const std::filesystem::path &mountPoint,
              FileRange dataRange, FileRange hashesRange, const std::vector<uint8_t> &rootHash,
              const std::vector<uint8_t> &salt, LIBRALF_NS::MountFlags flags) const override;

        // -------------------------------------------------------------------------
        /*!
            Runs sanity checks on the supplied image to verify the data and hashes
            are in the correct place and have valid sizes.  Also checks the
            dm-verity super block has sane values.

            This is primarily provided for unit tests, you don't need to call this
            prior to calling mount(), as that will run the same checks prior to
            mounting.

         */
        LIBRALF_NS::Result<> checkImageFile(FileSystemType fsType, const std::filesystem::path &imagePath,
                                            FileRange dataRange, FileRange hashesRange) const override;

        LIBRALF_NS::Result<> checkImageFile(FileSystemType fsType, int imageFd, FileRange dataRange,
                                            FileRange hashesRange) const override;
    };

} // namespace entos::ralf::dmverity