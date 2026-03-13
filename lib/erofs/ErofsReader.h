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

#include "ErofsInode.h"
#include "ErofsSuperBlock.h"
#include "IErofsImageFile.h"
#include "IErofsReader.h"

#include <filesystem>
#include <list>
#include <memory>
#include <vector>

namespace entos::ralf::erofs
{

    // -----------------------------------------------------------------------------
    /*!
        \class ErofsEntry
        \brief Simple wrapper around a path and a ErofsInode object.

        All the details, except the path, are stored in the ErofsInode object, this
        object is just a wrapper around that.

     */
    class ErofsEntry final : public IErofsEntry
    {
    public:
        ErofsEntry(std::shared_ptr<IErofsImageFile> file, const ErofsSuperBlock *superBlock,
                   std::filesystem::path &&path, ino_t inode, unsigned inodeDataHints);
        ~ErofsEntry() final = default;

        inline bool isValid() const { return m_inode.isValid(); }

        const std::filesystem::path &path() const override { return m_path; }

        std::filesystem::file_type type() const override { return m_inode.type(); }

        std::filesystem::perms permissions() const override { return m_inode.permissions(); }

        time_t modificationTime() const override { return m_inode.modificationTime(); }

        uid_t ownerId() const override { return m_inode.owner(); }

        gid_t groupId() const override { return m_inode.group(); }

        size_t size() const override
        {
            if (m_inode.type() == std::filesystem::file_type::directory)
                return 0;
            else
                return m_inode.size();
        }

        ssize_t read(void *buf, size_t size, size_t offset, LIBRALF_NS::Error *error) const override
        {
            return m_inode.read(buf, size, offset, error);
        }

    private:
        const std::filesystem::path m_path;
        const ErofsInode m_inode;
    };

    // -----------------------------------------------------------------------------
    /*!
        \class ErofsReader
        \brief Performs the EROFS image read by walking the directory tree.

        This starts at the root directory; reads the directory and processes all
        the entries, sub-directories are pushed onto a stack and files are processed
        and returned to the call via next() calls.

     */
    class ErofsReader final : public IErofsReader
    {
    public:
        explicit ErofsReader(std::shared_ptr<IErofsImageFile> file);
        ~ErofsReader() final = default;

        bool atEnd() const override;
        bool hasNext() const override;
        std::unique_ptr<IErofsEntry> next() override;

        bool hasError() const override;
        LIBRALF_NS::Error error() const override;

        AdviceFlags advice() const override;
        void setAdvice(AdviceFlags advice) override;

    private:
        void setError(std::error_code code, const char *format, ...) __attribute__((format(printf, 3, 4)));

        struct DirEntry
        {
            ino_t inode;
            std::string fileName;
            std::filesystem::file_type fileType;
        };

        std::optional<std::vector<DirEntry>> readDirectory(const std::filesystem::path &dirPath, ino_t dirInode);

        static bool processDirectoryBlock(const uint8_t *data, size_t dataSize,
                                          std::vector<ErofsReader::DirEntry> *entries);

        static bool validateFileName(const std::string &fileName);

    private:
        static constexpr size_t kMaxDirectorySize = (256 * 1024);

        const std::shared_ptr<IErofsImageFile> m_file;
        std::optional<ErofsSuperBlock> m_superBlock;

        /// Internal buffer used to read the directory entries
        std::vector<uint8_t> m_dirBlockBuffer;

        /// Error string set by setError, if not empty then hasError() returns true
        LIBRALF_NS::Error m_error;

        /// Stack of directories used when walking the fs tree
        struct Directory
        {
            std::filesystem::path path;
            ino_t inode;
        };
        std::list<Directory> m_dirStack;

        /// Stores the entries of the current directory we're walking
        std::vector<DirEntry> m_currentDir;
        size_t m_currentDirIndex = 0;

        /// Advice flags set by the user, these are used as hints to how the file will be read and can translate to
        /// posix_fdavise calls to optimise the flash reads in the kernel
        AdviceFlags m_adviceFlags;
    };

} // namespace entos::ralf::erofs