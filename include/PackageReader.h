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
#include "Package.h"
#include "PackageEntry.h"
#include "VerificationBundle.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace LIBRALF_NS
{
    class IPackageReaderImpl;

    // -----------------------------------------------------------------------------
    /*!
        \class PackageReader
        \brief Object used to read / extract files from a package.

        The object is designed to be used in a for loop type setup, for example
        a typical use case for iterating through a package would look like:

        \code
            Package package = Package::open("package.wgt", credentials);
            PackageReader reader = PackageReader::create(package);
            std::optional<PackageEntry> entry;
            while ((entry = reader.next()))
            {
                PackageEntry entry = reader.next();
                std::cout << "path:" << entry->path() << " type:" << entry->type() << std::endl;

                // to read the file
                if (entry->type() == std::filesystem::file_type::regular_file)
                {
                    entry->read(...);
                }
             }

             if (reader.hasError())
             {
                std::cerr << "failed to read package due to " << reader.error() << std::endl;
             }
        \endcode

        The reader is guaranteed to give you the parent directory entries of a file
        or symlink entry before the actual file or symlink entry.  This means you
        can create directories before creating files or symlinks.

        To improve performance iterating through the package only walks the
        directory tree, to read the contents of a file in the package you need to
        call PackageEntry::read(...).

        \important Returned PackageEntry objects are only valid until the next call
        to next() or until the reader object is destroyed.  You can not copy the
        PackageEntry object and should read the entire contents of the entry before
        calling next().

        \important Cryptographic verification is performed on the files in the
        package as they are read from the package, therefore the only way
        to verify that everything in the package is valid is to read every file,
        symlink and directory.  If you want to just verify a complete package -
        for example after downloading - then it's more optimal to use the
        Package::verify() API.

     */
    class LIBRALF_EXPORT PackageReader
    {
    public:
        // -------------------------------------------------------------------------
        /*!
            Creates a reader object for the given package.  Multiple readers can
            be created for a single package, each reader has it's own internal
            state.

            Verification credentials are used from the \a package object, to validate
            the files, symlinks and directories as they are read.

            If there was an error creating the PackageReader object then the
            resulting object's hasError() call will return \c true, and the error()
            method will return the reason.
         */
        static PackageReader create(const Package &package);

    public:
        PackageReader(const PackageReader &) = delete;
        PackageReader &operator=(const PackageReader &) = delete;

        ~PackageReader();

        // -------------------------------------------------------------------------
        /*!
            Returns the next entry in the package if there is one. If reached the
            end of the package, or there was an error processing the package then
            an \c std::nullopt optional is returned.

         */
        std::optional<PackageEntry> next();

        // -------------------------------------------------------------------------
        /*!
            Returns \c true if an error has occurred processing the package.  Once
            an error has occurred you should stop reading the package and destroy
            the object, it is not possible to recover from an error.

         */
        bool hasError() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the last error that occurred while processing the package.

         */
        Error error() const;

    protected:
        explicit PackageReader(std::unique_ptr<IPackageReaderImpl> &&impl);

    private:
        std::unique_ptr<IPackageReaderImpl> m_impl;
    };

} // namespace LIBRALF_NS
