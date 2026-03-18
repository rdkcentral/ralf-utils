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

#include "IOCIBackingStore.h"
#include "archive/LibarchiveFileReader.h"

#include <map>

// -------------------------------------------------------------------------
/*!
    \class OCIArchiveBackingStore
    \brief Implement of IOCIBackingStore that reads files from an archive.

    This wrapper uses the libarchive library to read files from an archive,
    it also implements a cache of small files to avoid processing the archive
    multiple times to get the small individual files in an OCI package.

*/
class OCIArchiveBackingStore final : public IOCIBackingStore
{
public:
    static LIBRALF_NS::Result<std::shared_ptr<OCIArchiveBackingStore>> open(int archiveFd, bool enableCache);

public:
    ~OCIArchiveBackingStore() final = default;

    int64_t size() const override;

    bool supportsMountableFiles() const override;

    LIBRALF_NS::Result<std::vector<uint8_t>> readFile(const std::filesystem::path &path, size_t maxSize) const override;

    LIBRALF_NS::Result<std::unique_ptr<IOCIFileReader>> getFile(const std::filesystem::path &path) const override;

    LIBRALF_NS::Result<std::unique_ptr<IOCIMappableFile>> getMappableFile(const std::filesystem::path &path) const override;

private:
    OCIArchiveBackingStore(std::unique_ptr<entos::ralf::archive::LibarchiveFileReader> &&archive, int64_t size,
                           bool enableCache);

    void cacheSmallFiles();

    static LIBRALF_NS::Result<std::vector<uint8_t>>
    readArchiveFileEntry(const std::unique_ptr<entos::ralf::archive::LibarchiveFileReader> &archive,
                         archive_entry *entry, int64_t maxSize);

    static LIBRALF_NS::Result<std::unique_ptr<IOCIMappableFile>>
    getTarArchiveEntryMappableFile(const std::unique_ptr<entos::ralf::archive::LibarchiveFileReader> &archive,
                                   archive_entry *entry);

    static LIBRALF_NS::Result<std::unique_ptr<IOCIMappableFile>>
    getZipArchiveEntryMappableFile(const std::unique_ptr<entos::ralf::archive::LibarchiveFileReader> &archive,
                                   archive_entry *entry);

private:
    const std::unique_ptr<entos::ralf::archive::LibarchiveFileReader> m_archive;
    const int64_t m_size;

    /// A cache of small files read from the archive
    std::map<std::filesystem::path, std::shared_ptr<std::vector<uint8_t>>> m_cache;
};
