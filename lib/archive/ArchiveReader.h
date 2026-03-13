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

#include "Error.h"
#include "LibRalf.h"
#include "core/IPackageReaderImpl.h"

#include <deque>
#include <filesystem>
#include <map>
#include <memory>

struct archive_entry;

namespace entos::ralf::archive
{
    class ILibarchiveReader;

    // -------------------------------------------------------------------------
    /*!
        \class ArchiveReader
        \brief Higher level archive reader that implements the IPackageReaderImpl
        interface.

        This takes a libarchive reader and wraps it in a package reader interface.
        It can also take an optional file digest map that is used to verify the
        files and symlinks as they're read from the archive.

     */
    class ArchiveReader : public LIBRALF_NS::IPackageReaderImpl
    {
    public:
        using FileDigestMap = std::map<std::filesystem::path, std::vector<uint8_t>>;

    public:
        explicit ArchiveReader(std::unique_ptr<ILibarchiveReader> &&archive,
                               std::shared_ptr<const FileDigestMap> digests = nullptr);
        ~ArchiveReader() override = default;

        std::unique_ptr<LIBRALF_NS::IPackageEntryImpl> next() override;

        bool hasError() const override;
        LIBRALF_NS::Error error() const override;

    private:
        void setError(LIBRALF_NS::ErrorCode code, const char *format, ...) __attribute__((format(printf, 3, 4)));

        std::unique_ptr<LIBRALF_NS::IPackageEntryImpl> createEntry(std::filesystem::path &&path, archive_entry *entry);

        void updateParentEntryStack(const std::filesystem::path &parentPath, time_t dirModTime);

        bool withinSymLink(const std::filesystem::path &path) const;

    private:
        const std::shared_ptr<const FileDigestMap> m_digests;

        friend class ArchiveDirectoryEntry;
        friend class ArchiveSymlinkEntry;
        friend class ArchiveFileEntry;

        struct FSTreeNode
        {
            std::map<std::filesystem::path, FSTreeNode> children;
        };
        FSTreeNode m_fstreeRoot;
        size_t m_fstreeSize;

        size_t m_maxArchiveDirectories;

        std::deque<std::unique_ptr<LIBRALF_NS::IPackageEntryImpl>> m_entryStack;

        std::deque<std::filesystem::path> m_symlinks;

        struct SharedReadState
        {
            std::unique_ptr<ILibarchiveReader> archive;
            LIBRALF_NS::Error error;
            bool endOfArchive;
            uint64_t entryIndex;

            explicit SharedReadState(std::unique_ptr<ILibarchiveReader> &&archive);
        };

        std::shared_ptr<SharedReadState> m_readState;
        size_t m_verifiedEntries;
    };

} // namespace entos::ralf::archive