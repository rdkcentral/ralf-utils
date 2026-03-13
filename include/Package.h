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
#include "LibRalf.h"
#include "PackageAuxMetaData.h"
#include "PackageEntry.h"
#include "PackageMetaData.h"
#include "PackageMount.h"
#include "Result.h"
#include "VerificationBundle.h"

#include <filesystem>
#include <list>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>

namespace LIBRALF_NS
{
    class IPackageImpl;

    // -------------------------------------------------------------------------
    /*!
        \class Package
        \brief Object representing a package (widget file).

        Provides a wrapper around a widget file with methods to verify, extract
        and mount the package.  With helpers to read the package meta-data.

     */
    class LIBRALF_EXPORT Package
    {
    public:
        enum class OpenFlag : uint32_t
        {
            None = 0,
            CheckCertificateExpiry = (1 << 0),
        };

        LIBRALF_DECLARE_ENUM_FLAGS(OpenFlags, OpenFlag)

        enum class Format : uint32_t
        {
            Unknown = 0,
            Widget = 1,
            Ralf = 2,
        };

        // -------------------------------------------------------------------------
        /*!
            Opens a package (widget) file at \a filePath, using the supplied
            \a credentials to verify the package contents when processed.

            \important Only limited package verification is performed by this call,
            and successfully opening the package DOES NOT indicate the entire
            package is valid.  If wanting to do a full verification, for example
            after downloading a package, then you should call verify() on the
            returned object.

            You should call isValid() after this call to verify that the package
            was successfully opened.

         */
        static Result<Package> open(const std::filesystem::path &filePath, const VerificationBundle &verificationBundle,
                                    OpenFlags flags = OpenFlags::None);

        // -------------------------------------------------------------------------
        /*!
            \overload

            Overload of the open() call that supplies the expect format of the package.
            This will skip the auto format detection and assume the package is the
            given type.

        */
        static Result<Package> open(const std::filesystem::path &filePath, const VerificationBundle &verificationBundle,
                                    OpenFlags flags, Format format);

        // -------------------------------------------------------------------------
        /*!
            Processes the open package given it's file descriptor.  This method
            does not take ownership of the file descriptor and the caller should
            close it when no longer required.  The file offset of the fd may be
            changed by this call or subsequent calls on the returned object.

            \important Only limited package verification is performed by this call,
            and successfully opening the package DOES NOT indicate the entire
            package is valid.  If wanting to do a full verification, for example
            after downloading a package, then you should call verify() on the
            returned object.

            You should call isValid() after this call to verify that the package
            was successfully opened.

         */
        static Result<Package> open(int packageFd, const VerificationBundle &verificationBundle,
                                    OpenFlags flags = OpenFlags::None);

        // -------------------------------------------------------------------------
        /*!
            \overload

            Overload of the open() call that supplies the expect format of the package.
            This will skip the auto format detection and assume the package is the
            given type.

         */
        static Result<Package> open(int packageFd, const VerificationBundle &verificationBundle, OpenFlags flags,
                                    Format format);

        // -------------------------------------------------------------------------
        /*!
            Opens the package without performing any cryptographic verification.

         */
        static Result<Package> openWithoutVerification(const std::filesystem::path &filePath,
                                                       OpenFlags flags = OpenFlags::None);

        // -------------------------------------------------------------------------
        /*!
            Opens the package given an fd, without performing any cryptographic
            verification.

        */
        static Result<Package> openWithoutVerification(int packageFd, OpenFlags flags = OpenFlags::None);

        // -------------------------------------------------------------------------
        /*!
            Attempts to detect the format of package from the supplied \a packageFd
            corresponding to the package file.

            If couldn't detect the package type then \a Package::Format::Unknown
            is returned.

            \warning This function does no cryptographical signature checks on the
            file, if a package type is returned it is based on a best guess by looking
            at file headers and / or archive contents.  A valid result should DOES NOT
            indicate the package is valid.

            This function mainly works by detecting common archive formats (zip, tar,
            etc) and then looking at the contents of said archive. This may be an
            expensive operation for archive types that require decompression to get
            a file list of the archive contents (like .tar.gz for instance).  For
            uncompressed tar and zip archives it it relatively cheap to get a content
            list of the archive.

         */
        static Package::Format detectPackageFormat(int packageFd);

        // -------------------------------------------------------------------------
        /*!
            \overload

            Overload of detectPackageFormat(int packageFd), that attempts to get the
            format of the package at \a filePath.

         */
        static Package::Format detectPackageFormat(const std::filesystem::path &filePath);

    public:
        enum class VerificationFlag : uint32_t
        {
            None = 0,
            CheckCertificateExpiry = (1 << 0),
        };

        LIBRALF_DECLARE_ENUM_FLAGS(VerificationFlags, VerificationFlag)

        // -------------------------------------------------------------------------
        /*!
            \enum ExtractOption

            Defines optional flags for the extractTo() method.  By default the
            extractTo method will overwrite any files in the target directory, and
            it will update the metadata of any existing directory.

            KeepExistingFiles - If a file already exists in the target directory then
            this causes the extractTo() method to fail and return an error.

            SkipExistingFiles - If a file already exists in the target directory then
            it is not replaced and the file is not extracted.

            NoOverwriteDirectories - If a directory already exists in the target
            directory then this flag ensures the meta-data (permissions, owner, etc)
            is not changed.
         */
        enum class ExtractOption : uint32_t
        {
            None = 0,
            KeepExistingFiles = (1 << 0),
            SkipExistingFiles = (1 << 1),
            NoOverwriteDirectories = (1 << 1),
        };

        LIBRALF_DECLARE_ENUM_FLAGS(ExtractOptions, ExtractOption)

    public:
        Package();
        Package(const Package &) = delete;
        Package(Package &&other) noexcept;
        ~Package();

        Package &operator=(const Package &) = delete;
        Package &operator=(Package &&other) noexcept;

        bool isValid() const;

        // -------------------------------------------------------------------------
        /*!
            Returns \c true if the package can be mounted on the current system.

            This doesn't check that the caller can perform the mount (ie. doesn't
            check caller is running as root or has the necessary capabilities).
            However it checks that the package is in a format that can be mounted
            and that the system supports mounting packages of this type.
         */
        bool isMountable() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the id of the package, aka the appId.  Will return an empty
            string if failed to open or verify the package.

        */
        std::string id() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the semantic version number of the package.  Not all package
            types use semantic versioning, so this may return an empty version
            ("0" version).

        */
        VersionNumber version() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the version name of the package.  Will return an empty string if
            failed to open or verify the package.

            Version name strings are free form and can be anything, including empty,
            so an empty string doesn't necessarily indicate an error.
        */
        std::string versionName() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the format of the package.
         */
        Format format() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the size of the package on disk.
         */
        ssize_t size() const;

        // -------------------------------------------------------------------------
        /*!
            Returns an estimate of the unpacked size of the package.

            It is an estimate because it only considers the size of the regular files
            and not directories or symlinks.
         */
        ssize_t unpackedSize() const;

        // -------------------------------------------------------------------------
        /*!
            Performs a full verification of the package contents.

            This is equivalent to creating a PackageReader object and reading all
            the entries in the package, and then checking for any read / verification
            errors.  However this may be more efficient on some package types if
            all you want to do is verify the complete package.

            It only makes sense to call this method if the package was opened with
            a verification bundle.  If the package was opened by calling
            Package::openWithoutVerification() then this method will always return
            \c false.

            Returns \c true if the package is verified, otherwise \c false.

            If \a error is supplied and the verification fails then it will be updated
            with the reason for the failure.
         */
        Result<> verify() const;

        // -------------------------------------------------------------------------
        /*!
            Extracts the package to the \a target directory.  If the package was
            opened with a verification bundle then the contents of the package will
            be verified as written to the target directory.

            On failure \c false is returned and \a error is updated with the reason.
            However the extraction may have partially completed, so you should check
            the target directory for any files that were extracted before the error.

            If the package was opened without a verification bundle then the contents
            are extracted without a cryptographic verification.  However checks are
            still performed on things like file paths and sizes.

            This method will not overwrite files in the target directory, if a file
            already exists in the \a target directory then the extraction will fail
            and return \c false.  Ideally the \a target directory should be empty.

            This method is equivalent to creating a PackageReader object and iterating
            through all files and writing them to the target directory.  However it
            may be more efficient on some package types as it can bypass the overhead
            of using the PackageReader.

         */
        Result<> extractTo(const std::filesystem::path &target, ExtractOptions options = ExtractOption::None) const;

        // -------------------------------------------------------------------------
        /*!
            Same as extractTo() that takes a directory path, but takes \a targetDirFd
            which is a file descriptor to the target directory.

         */
        Result<> extractTo(int targetDirFd, ExtractOptions options = ExtractOption::None) const;

        // -------------------------------------------------------------------------
        /*!
            Mounts the package at the given \a mountPoint.  The mount point must
            be a valid directory path and must already exist.

            \note All packages are mounted read-only, regardless of the flags.

         */
        Result<PackageMount> mount(const std::filesystem::path &mountPoint,
                                   MountFlags flags = (MountFlag::ReadOnly | MountFlag::NoSuid)) const;

        // -------------------------------------------------------------------------
        /*!
            Returns the package meta-data object. If the package was opened with a
            verification bundle then the meta-data will be verified as read and the
            returned object will be signature checked.

            If the package was opened without a verification bundle then the meta-data
            is read without any cryptographic verification.  However checks are still
            performed on things like file paths and sizes.

         */
        Result<PackageMetaData> metaData() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the signing certificates from the package.

        */
        Result<std::list<Certificate>> signingCertificates() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the contents of an auxiliary meta data files in the package.

            Auxiliary meta data files are optional files that can be included in the
            package, they are identified by their mediaType, but note there may be
            multiple files with the same media type in the package.
         */
        Result<PackageAuxMetaData> auxMetaDataFile(std::string_view mediaType, size_t index = 0) const;

        // -------------------------------------------------------------------------
        /*!
            Returns the number of auxiliary meta data files in the package for the
            given \a mediaType.  If there are no files with the given media type then
            this will return 0.

         */
        Result<size_t> auxMetaDataFileCount(std::string_view mediaType) const;

        // -------------------------------------------------------------------------
        /*!
            Returns a set of available auxiliary meta data media types in the package.

        */
        Result<std::set<std::string>> auxMetaDataKeys() const;

        // -------------------------------------------------------------------------
        /*!
            Sets the limits for extracting packages. \a maxTotalSize is the maximum
            total size of all files in bytes that can be extracted from the package,
            and \a maxFileCount is the maximum number of files, directories or symlinks
            that can be extracted.

            If the package exceeds either of these limits then the verify() and extractTo()
            methods will fail and return an error.

            By default there are no limits on extraction.
         */
        size_t maxExtractionBytes() const;
        void setMaxExtractionBytes(size_t maxTotalSize);

        size_t maxExtractionEntries() const;
        void setMaxExtractionEntries(size_t maxFileCount);

    protected:
        friend class PackageReader;

        explicit Package(std::unique_ptr<IPackageImpl> &&impl);

    private:
        std::unique_ptr<IPackageImpl> m_impl;
    };

    LIBRALF_EXPORT std::ostream &operator<<(std::ostream &s, const Package::Format &format);

    LIBRALF_DECLARE_ENUM_FLAGS_OPERATORS(Package::OpenFlags)
    LIBRALF_DECLARE_ENUM_FLAGS_OPERATORS(Package::VerificationFlags)
    LIBRALF_DECLARE_ENUM_FLAGS_OPERATORS(Package::ExtractOptions)

} // namespace LIBRALF_NS
