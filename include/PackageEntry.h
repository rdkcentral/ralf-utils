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

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <sys/types.h>

namespace LIBRALF_NS
{
    class IPackageEntryImpl;

    // -------------------------------------------------------------------------
    /*!
        \class PackageEntry
        \brief Object that refers to a filesystem entity within a package, it
        may be a directory, symlink or regular file.



     */
    class LIBRALF_EXPORT PackageEntry
    {
    public:
        PackageEntry(const PackageEntry &) = delete;
        PackageEntry(PackageEntry &&other) noexcept;

        PackageEntry &operator=(const PackageEntry &) = delete;
        PackageEntry &operator=(PackageEntry &&other) noexcept;

        ~PackageEntry();

        // -------------------------------------------------------------------------
        /*!
            The entry's path relative to the root of the package.  The path
            doesn't include a leading '/', which means the root directory entry's
            path will be empty.

         */
        const std::filesystem::path &path() const;

        // -------------------------------------------------------------------------
        /*!
            If the entry is a regular file type, then this is the (decompressed)
            size of the file.

            For symlink types this is the length of the symlink target path.

            For directories this is ignored and size will always be 0 for directory
            entries.

         */
        size_t size() const;

        // -------------------------------------------------------------------------
        /*!
            The permissions on the entry, this applies to directories, regular files
            and symlinks.  Although for symlinks they generally take the permissions
            of what they are pointing to.

         */
        std::filesystem::perms permissions() const;

        // -------------------------------------------------------------------------
        /*!
            The modification time, in epoc seconds, of the entry as recorded in the
            package.

         */
        time_t modificationTime() const;

        // -------------------------------------------------------------------------
        /*!
            The uid of the owner of the file.

         */
        uid_t ownerId() const;

        // -------------------------------------------------------------------------
        /*!
            The gid of the group of the file.

         */
        gid_t groupId() const;

        // -------------------------------------------------------------------------
        /*!
            The type of the entry, currently only support the following types:
                - std::filesystem::file_type::directory
                - std::filesystem::file_type::regular
                - std::filesystem::file_type::symlink

         */
        std::filesystem::file_type type() const;

        inline bool isRegularFile() const { return (type() == std::filesystem::file_type::regular); }

        inline bool isDirectory() const { return (type() == std::filesystem::file_type::directory); }

        inline bool isSymLink() const { return (type() == std::filesystem::file_type::symlink); }

        // -------------------------------------------------------------------------
        /*!
            Reads the content of the entry.  This shouldn't be called for directory
            entries, if you do it will return \c -1.

            For regular files this behaves the same as a read() system call on a
            regular file.  In particular, this will read data sequentially from
            the package entry, starting at the current offset, and will advance the
            offset by the number of bytes read.

            For symlinks the data read is the symlink target, for example to create
            a real symlink from this entry you could do this (sans error checking):

                \code
                    char target[PATH_MAX];
                    ssize_t rd = entry.read(target, PATH_MAX - 1);
                    target[rd] = '\0';

                    symlink(target, entry->path().c_str());
                \endcode

            Returns the number of bytes read or 0 if reached the end of the entry.
            On any error -1 is returned and \a error is set.

            Note that there is no way to seek within the entry, or rewind back to the
            start of the entry, this is due to the sequential nature of the package
            storage.
         */
        ssize_t read(void *buf, size_t size, Error *error = nullptr);

    protected:
        friend class PackageReader;

        PackageEntry();
        explicit PackageEntry(std::unique_ptr<IPackageEntryImpl> &&impl);

    private:
        std::unique_ptr<IPackageEntryImpl> m_impl;
    };

} // namespace LIBRALF_NS
