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

#include "IDmVerityVerifier.h"

#include "LibRalf.h"
#include "Result.h"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace entos::ralf::dmverity
{
    class IHashTreeLayer;

    class DmVerityVerifier : public IDmVerityVerifier
    {
    public:
        DmVerityVerifier() = default;
        ~DmVerityVerifier() override;

        LIBRALF_NS::Result<> open(int hashesFileFd, const std::vector<uint8_t> &rootHash,
                                  std::optional<size_t> hashesOffset = std::nullopt,
                                  std::optional<size_t> hashesSize = std::nullopt);

        size_t blockSize() const override;

        size_t dataBlockCount() const override;

        bool verify(size_t block, const void *data) const override;

    private:
        LIBRALF_NS::Result<> parseDmVeritySuperBlock(int fd);

    private:
        std::array<uint8_t, 32> m_rootHash = { 0 };

        int m_hashesFd = -1;
        size_t m_hashesOffset = 0;
        size_t m_hashesSize = 0;

        size_t m_hashBlockSize = 0;
        size_t m_hashesPerBlock = 0;
        size_t m_dataBlockSize = 0;
        size_t m_dataBlockCount = 0;

        std::vector<uint8_t> m_salt;

        /// This is the merkle hash tree, this stores the lowest layer and each layer
        /// stores a pointer to it's parent
        std::unique_ptr<IHashTreeLayer> m_layer0;
    };

} // namespace entos::ralf::dmverity