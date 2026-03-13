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

#include "dm-verity/IDmVerityMounter.h"

#include <gmock/gmock.h>

namespace entos::ralf::dmverity
{

    class MockDmVerityMounter : public IDmVerityMounter
    {
    public:
        MOCK_METHOD(Result<std::unique_ptr<IPackageMountImpl>>, mount,
                    (std::string_view name, FileSystemType fsType, const std::filesystem::path &imagePath,
                     const std::filesystem::path &mountPoint, FileRange dataRange, FileRange hashesRange,
                     const std::vector<uint8_t> &rootHash, const std::vector<uint8_t> &salt, MountFlags flags),
                    (const, override));

        MOCK_METHOD(Result<std::unique_ptr<IPackageMountImpl>>, mount,
                    (std::string_view name, FileSystemType fsType, int imageFd, const std::filesystem::path &mountPoint,
                     FileRange dataRange, FileRange hashesRange, const std::vector<uint8_t> &rootHash,
                     const std::vector<uint8_t> &salt, MountFlags flags),
                    (const, override));

        MOCK_METHOD(Result<>, checkImageFile,
                    (FileSystemType fsType, const std::filesystem::path &imagePath, FileRange dataRange,
                     FileRange hashesRange),
                    (const, override));

        MOCK_METHOD(Result<>, checkImageFile,
                    (FileSystemType fsType, int imageFd, FileRange dataRange, FileRange hashesRange), (const, override));
    };

} // namespace entos::ralf::dmverity
