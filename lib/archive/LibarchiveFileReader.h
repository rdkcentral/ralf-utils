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

#include <memory>
#include <vector>

namespace entos::ralf::archive
{

    // -------------------------------------------------------------------------
    /*!
        \class LibarchiveFileReader
        \brief Lightweight wrapper around libarchive to read files from an archive.

        This class reads an archive given a file descriptor, however it uses the
        pread() function. This allows multiple archive readers to be used on the
        same file descriptor without interference, that would happen if they
        adjusted the file offset using ordinary read() calls.

     */
    class LibarchiveFileReader final : public ILibarchiveReader
    {
    public:
        LibarchiveFileReader(int archiveFd, size_t archiveSize, ArchiveFormats requiredFormats, bool autoCloseFd,
                             LIBRALF_NS::Error *_Nullable error);

        ~LibarchiveFileReader() final;

        std::unique_ptr<LibarchiveFileReader> clone() const;

        int fd() const;

        int rewind();

        int nextHeader(archive_entry *_Nullable *_Nullable entry) override;

        ssize_t readData(void *_Nullable buf, size_t size) override;

        int readDataBlock(const void *_Nullable *_Nullable buff, size_t *_Nullable size,
                          int64_t *_Nullable offset) override;

        off_t seekData(off_t offset, int whence);

        const char *_Nonnull errorString() override;

        bool isNull() const;

        ArchiveFormat format() const override;

        int64_t headerOffset() const;

    private:
        LIBRALF_NS::Result<struct archive *> openArchive();

        static la_ssize_t readCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData,
                                       const void *_Nullable *_Nonnull outBuffer);

        static la_int64_t skipCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData, la_int64_t skip);

        static la_int64_t seekCallback(struct archive *_Nonnull aArchive, void *_Nonnull userData, la_int64_t offset,
                                       int whence);

    private:
        int m_fd;
        la_int64_t m_size;
        const ArchiveFormats m_requiredFormats;
        const bool m_autoCloseFd;

        struct archive *_Nullable m_archive = nullptr;

        la_int64_t m_offset = 0;
        std::vector<uint8_t> m_buffer;
    };

} // namespace entos::ralf::archive