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

#include "IErofsImageFile.h"

#include "EnumFlags.h"
#include "Error.h"
#include "Result.h"

#include <filesystem>
#include <memory>

#include <sys/types.h>

namespace entos::ralf::erofs
{

    // -----------------------------------------------------------------------------
    /*!
        \interface IErofsEntry
        \brief Interface representing a entry in the EROFS image.

        An entry can be a directory, regular file or symlink (other file types like
        character devices, sockets, fifos, etc are supported by EROFS but this code
        currently ignore them).

     */
    class IErofsEntry
    {
    public:
        virtual ~IErofsEntry() = default;

        // -------------------------------------------------------------------------
        /*!
            The entry's path relative to the root of the EROFS image.  The path
            doesn't include a leading '/', which means the root directory entry's
            path will be empty.

         */
        virtual const std::filesystem::path &path() const = 0;

        // -------------------------------------------------------------------------
        /*!
            If the entry is a regular file type, then this is the (decompressed)
            size of the file.

            For symlink types this is the length of the symlink target path.

            For directories this is ignore and size will always be 0 for directory
            entries.

         */
        virtual size_t size() const = 0;

        // -------------------------------------------------------------------------
        /*!
            The permissions on the entry, this applies to directories, regular files
            and symlinks.  Although for symlinks they generally take the permissions
            of what they are pointing to.

         */
        virtual std::filesystem::perms permissions() const = 0;

        // -------------------------------------------------------------------------
        /*!
            The modification time, in epoc seconds, of the entry as recorded in the
            EROFS image.

         */
        virtual time_t modificationTime() const = 0;

        // -------------------------------------------------------------------------
        /*!
            The id of the owner of the file.

         */
        virtual uid_t ownerId() const = 0;

        // -------------------------------------------------------------------------
        /*!
            The id of the group of the file.

         */
        virtual gid_t groupId() const = 0;

        // -------------------------------------------------------------------------
        /*!
            The type of the entry, currently only support the following types:
                - std::filesystem::file_type::directory
                - std::filesystem::file_type::regular
                - std::filesystem::file_type::symlink

         */
        virtual std::filesystem::file_type type() const = 0;

        inline bool isRegularFile() const { return (type() == std::filesystem::file_type::regular); }

        inline bool isDirectory() const { return (type() == std::filesystem::file_type::directory); }

        inline bool isSymLink() const { return (type() == std::filesystem::file_type::symlink); }

        // -------------------------------------------------------------------------
        /*!
            Reads the content of the entry.  This shouldn't be called for directory
            entries, if you do it will return \c -1.

            For regular files this behaves the same as a pread() call on a regular
            file.

            For symlinks the data read is the symlink target, for example to create
            a real symlink from this entry you could do this (sans error checking):

                \code
                    char target[PATH_MAX];
                    ssize_t rd = entry->readData(target, PATH_MAX - 1, 0);
                    target[rd] = '\0';

                    symlink(target, entry->path().c_str());
                \endcode

            On any error -1 is returned and \c errno is set.

         */
        virtual ssize_t read(void *buf, size_t size, size_t offset, LIBRALF_NS::Error *error) const = 0;
    };

    // -----------------------------------------------------------------------------
    /*!
        \interface IErofsReader
        \brief Object use to read / extract files from a EROFS image.

        The object is designed to be used in a for loop type setup, for example
        a typical use case for iterating through a file would look like:

        \code
            IErofsReader *reader = IErofsReader::create("some.erofs.img");
            while (reader->hasNext())
            {
                std::unique_ptr<IErofsEntry> entry = reader.next();
                std::cout << "path:" << entry->path() << " type:" << entry->type() << std::endl;

                // to read the file
                if (entry->type() == std::filesystem::file_type::regular_file)
                {
                    entry->read(...);
                }
             }

             if (reader->hasError())
             {
                std::cerr << "failed to read erofs image due to " << reader->error() << std::endl;
             }
        \endcode

        The reader is guaranteed to give you the parent directory entry of a file
        before the actual file / symlink entry itself.

        To improve performance iterating through the EROFS image only walks the
        directory tree, to read the contents of a file in the image you need to
        call IErofsEntry::read(...).

     */
    class IErofsReader
    {
    public:
        enum class Advice : unsigned
        {
            None = 0,
            ReadAllOnce = (1 << 0),
        };
        LIBRALF_DECLARE_ENUM_FLAGS(AdviceFlags, Advice)

    public:
        static LIBRALF_NS::Result<std::shared_ptr<IErofsReader>> create(const std::filesystem::path &imageFile);
        static LIBRALF_NS::Result<std::shared_ptr<IErofsReader>> create(std::shared_ptr<IErofsImageFile> imageFile);

    public:
        virtual ~IErofsReader() = default;

        virtual bool atEnd() const = 0;
        virtual bool hasNext() const = 0;
        virtual std::unique_ptr<IErofsEntry> next() = 0;

        virtual bool hasError() const = 0;
        virtual LIBRALF_NS::Error error() const = 0;

        virtual AdviceFlags advice() const = 0;
        virtual void setAdvice(AdviceFlags advice) = 0;
    };

    LIBRALF_DECLARE_ENUM_FLAGS_OPERATORS(IErofsReader::AdviceFlags)

} // namespace entos::ralf::erofs