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

#include "LibarchiveCommon.h"
#include "core/Compatibility.h"

#include <archive.h>
#include <archive_entry.h>

#include <filesystem>

namespace entos::ralf::archive
{

    class ILibarchiveReader
    {
    public:
        virtual ~ILibarchiveReader() = default;

        virtual ArchiveFormat format() const = 0;

        virtual int nextHeader(archive_entry *_Nullable *_Nullable entry) = 0;

        virtual ssize_t readData(void *_Nullable buf, size_t size) = 0;

        virtual int readDataBlock(const void *_Nullable *_Nullable buff, size_t *_Nullable size,
                                  int64_t *_Nullable offset) = 0;

        virtual const char *_Nonnull errorString() = 0;
    };

} // namespace entos::ralf::archive