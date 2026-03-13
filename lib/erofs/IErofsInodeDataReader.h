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

#include <sys/types.h>

namespace entos::ralf::erofs
{

    // -----------------------------------------------------------------------------
    /*!
        \interface IErofsInodeDataReader
        \brief Provides an interface for all EROFS data readers to use.

        This is used by ErofsInode which creates a different DataReader object
        based on the format of the data (uncompressed, compressed,
        compressed-compacted, etc).

     */
    class IErofsInodeDataReader
    {
    public:
        enum DataAccessAdvice : unsigned
        {
            ReadDataSequentially = (1 << 0), ///< this translates to a fadvice call to indicate
                                             ///  we'll be reading all the data blocks sequentially
            ReadDataOnce = (1 << 1),         ///< this means on inode reader destruction we'll
                                             ///  clear the read pages from the page cache
        };

    public:
        virtual ~IErofsInodeDataReader() = default;

        virtual bool read(void *buf, size_t size, size_t offset) = 0;
    };

} // namespace entos::ralf::erofs