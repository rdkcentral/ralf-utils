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
#include "Result.h"

#include <archive.h>
#include <archive_entry.h>

namespace entos::ralf::archive
{
    enum class ArchiveFormat : unsigned
    {
        Unknown = 0,

        Tarball = (1 << 0),
        TarballGzip = (1 << 1),
        TarballZstd = (1 << 2),

        Zip = (1 << 8),
        ZipStream = (1 << 9),

        All = (Tarball | TarballGzip | TarballZstd | Zip | ZipStream),
    };

    LIBRALF_ENUM_FLAGS(ArchiveFormats, ArchiveFormat)

    LIBRALF_NS::Result<struct archive *> createArchiveReader(ArchiveFormats formats);

    ArchiveFormat archiveFormat(struct archive *);

} // namespace entos::ralf::archive