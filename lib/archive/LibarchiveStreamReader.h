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

#include "ILibarchiveReader.h"
#include "LibarchiveCommon.h"

#include "EnumFlags.h"
#include "Result.h"
#include "core/Compatibility.h"

#include <archive.h>
#include <archive_entry.h>

#include <functional>

namespace entos::ralf::archive
{

    // -------------------------------------------------------------------------
    /*!
        \class LibarchiveFileReader
        \brief Lightweight wrapper around libarchive to read an archive stream.

        At a minimum a read function function needs to be supplied, this will be
        called to read the data for the archive.

     */
    class LibarchiveStreamReader final : public ILibarchiveReader
    {
    public:
        using ReadFunction = std::function<ssize_t(const void *_Nonnull *_Nonnull buffer)>;
        using SkipFunction = std::function<int64_t(int64_t skip)>;

    public:
        LibarchiveStreamReader(ReadFunction readFunction, SkipFunction skipFunction, ArchiveFormats requiredFormats,
                               LIBRALF_NS::Error *_Nullable error);
        ~LibarchiveStreamReader() final;

        ArchiveFormat format() const override;

        int nextHeader(archive_entry *_Nullable *_Nullable entry) override;

        ssize_t readData(void *_Nullable buf, size_t size) override;

        int readDataBlock(const void *_Nullable *_Nullable buff, size_t *_Nullable size,
                          int64_t *_Nullable offset) override;

        const char *_Nonnull errorString() override;

        bool isNull() const;

    private:
        static la_ssize_t readCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData,
                                       const void *_Nullable *_Nonnull outBuffer);
        static la_int64_t skipCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData, la_int64_t skip);

    private:
        const ReadFunction m_streamReadFn;
        const SkipFunction m_streamSkipFn;

        struct archive *_Nullable m_archive = nullptr;
    };

} // namespace entos::ralf::archive