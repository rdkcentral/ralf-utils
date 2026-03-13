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

#include "ErofsTypes.h"
#include "Result.h"

#include <bitset>
#include <optional>
#include <string>
#include <vector>

#include <sys/types.h>

namespace entos::ralf::erofs
{

    // -----------------------------------------------------------------------------
    /*!
        \class ErofsSuperBlock
        \brief Utility wrapper around the EROFS superblock structure.

        Simply parses an validates a superblock structure and then provides some
        simple helper methods.

     */
    class ErofsSuperBlock
    {
    public:
        static LIBRALF_NS::Result<ErofsSuperBlock> parse(const void *data, size_t dataLen);

    public:
        ~ErofsSuperBlock() = default;

        std::string toString() const;

        inline ino_t rootInode() const { return static_cast<ino_t>(m_superBlock.rootInode); }

        inline size_t inodeOffset(ino_t inode) const
        {
            return (m_superBlock.metaBlockAddr * m_blockSize) + (32 * inode);
        }

        inline size_t blockSize() const { return m_blockSize; }

        enum class Feature : size_t
        {
            ZeroPadding = 0,
            SuperBlockChecksum = 1,
            ModificationTime = 2,
            XattrFilter = 3,

            Max
        };

        bool hasFeature(Feature feature) const { return m_features.test(static_cast<size_t>(feature)); }

        inline timespec buildTime() const
        {
            return { static_cast<time_t>(m_superBlock.buildTimeSec), static_cast<long>(m_superBlock.buildTimeNSec) };
        }

    private:
        explicit ErofsSuperBlock(const ErofsSuperBlockStruct *sb);

    private:
        ErofsSuperBlockStruct m_superBlock;
        size_t m_blockSize;

        std::bitset<static_cast<size_t>(Feature::Max)> m_features;
    };

} // namespace entos::ralf::erofs