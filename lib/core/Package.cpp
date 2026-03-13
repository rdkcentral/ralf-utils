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

#include "Package.h"
#include "IPackageEntryImpl.h"
#include "IPackageImpl.h"
#include "LogMacros.h"
#include "archive/LibarchiveFileReader.h"

#if defined(ENABLE_W3C_WIDGET)
#    include "w3c-widget/W3CPackage.h"
#endif
#if defined(ENABLE_OCI)
#    include "oci/OCIPackage.h"
#endif

#include <climits>
#include <cstring>
#include <deque>
#include <fstream>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

Package::Format Package::detectPackageFormat(int packageFd)
{
    // Currently we only support W3C widgets and OCI packages, both of these are tar or zip archives and to determine
    // which we have to look inside the archive for certain files.  This is potentially expensive for archives like
    // tar.gz which have to be decompressed to look inside, but there is no real alternative.

    struct stat st = {};
    if ((fstat(packageFd, &st) < 0) || (st.st_size <= 0))
    {
        logSysWarning(errno, "Failed to get package file size");
        return Format::Unknown;
    }

    Error error;
    LibarchiveFileReader archive(packageFd, st.st_size, ArchiveFormat::All, false, &error);
    if (archive.isNull() || error)
    {
        logWarning("Failed to open package archive - %s", error.what());
        return Format::Unknown;
    }

    Format format = Format::Unknown;

    archive_entry *entry = nullptr;
    while (archive.nextHeader(&entry) == ARCHIVE_OK)
    {
        const mode_t type = (archive_entry_filetype(entry) & AE_IFMT);
        if (type != AE_IFREG)
            continue;
        const char *entryPath = archive_entry_pathname(entry);
        if (!entryPath)
            continue;

        if (strcmp(entryPath, "oci-layout") == 0)
        {
            format = Package::Format::Ralf;
            break;
        }
        else if (strcmp(entryPath, "signature1.xml") == 0)
        {
            format = Package::Format::Widget;
            break;
        }
    }

    return format;
}

Package::Format Package::detectPackageFormat(const std::filesystem::path &filePath)
{
    int fd = ::open(filePath.c_str(), O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        logSysWarning(errno, "Failed to get package file '%s'", filePath.c_str());
        return Format::Unknown;
    }

    auto format = detectPackageFormat(fd);

    close(fd);

    return format;
}

static Result<std::unique_ptr<IPackageImpl>> openPackageImpl(int packageFd,
                                                             const std::optional<VerificationBundle> &bundle,
                                                             Package::OpenFlags flags, Package::Format format)
{
    Error error;

#if defined(ENABLE_OCI)
    if (format == Package::Format::Ralf)
    {
        auto impl = OCIPackage::open(packageFd, bundle, flags, &error);
        if (!impl || error)
            return error;
        else
            return impl;
    }
#endif
#if defined(ENABLE_W3C_WIDGET)
    if (format == Package::Format::Widget)
    {
        auto impl = W3CPackage::open(packageFd, bundle, flags, &error);
        if (!impl || error)
            return error;
        else
            return impl;
    }
#endif

    return Error(ErrorCode::InvalidPackage, "Unknown or unsupported package format");
}

Result<Package> Package::open(const std::filesystem::path &filePath, const VerificationBundle &bundle, OpenFlags flags)
{
    int fd = ::open(filePath.c_str(), O_CLOEXEC | O_RDONLY);
    if (fd < 0)
        return Error(std::error_code(errno, std::system_category()));

    auto package = open(fd, bundle, flags);

    close(fd);

    return package;
}

Result<Package> Package::open(const std::filesystem::path &filePath, const VerificationBundle &verificationBundle,
                              OpenFlags flags, Format format)
{
    int fd = ::open(filePath.c_str(), O_CLOEXEC | O_RDONLY);
    if (fd < 0)
        return Error(std::error_code(errno, std::system_category()));

    auto package = open(fd, verificationBundle, flags, format);

    close(fd);

    return package;
}

Result<Package> Package::open(int packageFd, const VerificationBundle &verificationBundle, OpenFlags flags, Format format)
{
    auto impl = openPackageImpl(packageFd, verificationBundle, flags, format);
    if (impl.is_error())
        return impl.error();

    return Package(std::move(impl.value()));
}

Result<Package> Package::open(int packageFd, const VerificationBundle &verificationBundle, OpenFlags flags)
{
    const Format format = detectPackageFormat(packageFd);
    if (format == Format::Unknown)
        return Error(ErrorCode::InvalidPackage, "Unknown package format");

    auto impl = openPackageImpl(packageFd, verificationBundle, flags, format);
    if (impl.is_error())
        return impl.error();

    return Package(std::move(impl.value()));
}

Result<Package> Package::openWithoutVerification(const std::filesystem::path &filePath, OpenFlags flags)
{
    int fd = ::open(filePath.c_str(), O_CLOEXEC | O_RDONLY);
    if (fd < 0)
        return Error(std::error_code(errno, std::system_category()));

    auto package = openWithoutVerification(fd, flags);

    close(fd);

    return package;
}

Result<Package> Package::openWithoutVerification(int packageFd, OpenFlags flags)
{
    const Format format = detectPackageFormat(packageFd);
    if (format == Format::Unknown)
        return Error(ErrorCode::InvalidPackage, "Unknown package format");

    auto impl = openPackageImpl(packageFd, std::nullopt, flags, format);
    if (impl.is_error())
        return impl.error();

    return Package(std::move(impl.value()));
}

Package::Package() // NOLINT(modernize-use-equals-default)
{
}

Package::Package(std::unique_ptr<IPackageImpl> &&impl)
    : m_impl(std::move(impl))
{
}

Package::Package(Package &&other) noexcept
    : m_impl(std::move(other.m_impl))
{
}

Package::~Package() // NOLINT(modernize-use-equals-default)
{
}

Package &Package::operator=(Package &&other) noexcept
{
    m_impl = std::move(other.m_impl);
    return *this;
}

bool Package::isValid() const
{
    return m_impl != nullptr;
}

// -------------------------------------------------------------------------
/*!
    This is a convenience method to get the package meta data and return the
    id field from that.

    To be efficient this assumes the underlying IPackageImpl object caches
    the meta data internally.
 */
std::string Package::id() const
{
    if (!m_impl)
        return {};

    auto metaData = m_impl->metaData(nullptr);
    if (!metaData)
        return {};
    else
        return metaData->id();
}

// -------------------------------------------------------------------------
/*!
    This is a convenience method to get the package meta data and return the
    version field from that.

    To be efficient this assumes the underlying IPackageImpl object caches
    the meta data internally.
 */
VersionNumber Package::version() const
{
    if (!m_impl)
        return {};

    auto metaData = m_impl->metaData(nullptr);
    if (!metaData)
        return {};
    else
        return metaData->version();
}

// -------------------------------------------------------------------------
/*!
    This is a convenience method to get the package meta data and return the
    version name field from that.

    To be efficient this assumes the underlying IPackageImpl object caches
    the meta data internally.
 */
std::string Package::versionName() const
{
    if (!m_impl)
        return {};

    auto metaData = m_impl->metaData(nullptr);
    if (!metaData)
        return {};
    else
        return metaData->versionName();
}

Package::Format Package::format() const
{
    if (!m_impl)
        return Format::Unknown;
    else
        return m_impl->format();
}

bool Package::isMountable() const
{
    if (!m_impl)
        return false;
    else
        return m_impl->isMountable();
}

ssize_t Package::size() const
{
    if (!m_impl)
        return -1;
    else
        return m_impl->size();
}

ssize_t Package::unpackedSize() const
{
    if (!m_impl)
        return -1;
    else
        return m_impl->unpackedSize();
}

Result<> Package::verify() const
{
    if (!m_impl)
    {
        return Error(ErrorCode::InvalidPackage, "Invalid Package object");
    }

    Error error;
    if (!m_impl->verify(&error))
    {
        return error;
    }

    return Ok();
}

// -------------------------------------------------------------------------
/*!
    Uses the internal IPackageReaderImpl and IPackageEntryImpl objects to
    extract the entire package (with verification) to the target directory.

*/
Result<> Package::extractTo(int targetDirFd, ExtractOptions options) const
{
    if (!m_impl)
    {
        return Error(ErrorCode::InvalidPackage, "Invalid Package object");
    }

    Error error;

    auto readerImpl = m_impl->createReader(&error);
    if (!readerImpl)
    {
        // Error already populated
        return error;
    }

    const uint64_t sizeLimit = m_impl->maxExtractionBytes();
    const uint64_t entriesLimit = m_impl->maxExtractionEntries();

    uint64_t totalExtracted = 0;
    uint64_t totalEntries = 0;

    std::deque<std::pair<std::filesystem::path, time_t>> dirModTimes;

    std::unique_ptr<IPackageEntryImpl> entry;
    while ((entry = readerImpl->next()))
    {
        const auto entrySize = entry->size();
        if ((totalExtracted + entrySize) > sizeLimit)
            return Error(ErrorCode::PackageExtractLimitExceeded, "Package extraction size limit exceeded");

        auto result = entry->writeTo(targetDirFd, entrySize, options);
        if (!result)
            return result.error();

        totalExtracted += result.value();

        if (++totalEntries >= entriesLimit)
            return Error(ErrorCode::PackageExtractLimitExceeded, "Package extraction entry limit exceeded");

        if (entry->type() == std::filesystem::file_type::directory)
            dirModTimes.emplace_back(entry->path(), entry->modificationTime());
    }

    if (readerImpl->hasError())
    {
        return readerImpl->error();
    }

    // Post process the directories to set their modification times, we can't do this as they're extracted as adding
    // files to the directory would update the mtime.
    for (const auto &dirModTime : dirModTimes)
    {
        const std::filesystem::path &dirPath = dirModTime.first;
        const time_t mtime = dirModTime.second;

        struct timespec times[2];
        times[0].tv_sec = 0;
        times[0].tv_nsec = UTIME_OMIT;
        times[1].tv_sec = mtime;
        times[1].tv_nsec = 0;

        if (utimensat(targetDirFd, dirPath.c_str(), times, 0) != 0)
        {
            logSysWarning(errno, "Failed to set modification time for directory '%s'", dirPath.c_str());
        }
    }

    return Ok();
}

Result<> Package::extractTo(const std::filesystem::path &target, ExtractOptions options) const
{
    int targetDirFd = ::open(target.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (targetDirFd < 0)
    {
        return Error(std::error_code(errno, std::system_category()), "Failed to open target extraction directory");
    }

    auto result = extractTo(targetDirFd, options);

    if (close(targetDirFd) != 0)
    {
        logSysError(errno, "Failed to close target directory");
    }

    return result;
}

Result<PackageMount> Package::mount(const std::filesystem::path &mountPoint, MountFlags flags) const
{
    if (!m_impl)
    {
        return Error(ErrorCode::InvalidPackage, "Invalid Package object");
    }

    auto mountImpl = m_impl->mount(mountPoint, flags);
    if (!mountImpl)
    {
        return mountImpl.error();
    }

    return PackageMount(std::move(mountImpl.value()));
}

Result<PackageMetaData> Package::metaData() const
{
    if (!m_impl)
    {
        return Error(ErrorCode::InvalidPackage, "Invalid Package object");
    }

    Error error;
    auto metaData = m_impl->metaData(&error);
    if (!metaData || error)
    {
        return error;
    }

    return PackageMetaData(metaData);
}

Result<std::list<Certificate>> Package::signingCertificates() const
{
    if (!m_impl)
    {
        return Error(ErrorCode::InvalidPackage, "Invalid Package object");
    }

    Error error;
    auto certs = m_impl->signingCertificates(&error);
    if (error)
    {
        return error;
    }

    return certs;
}

Result<PackageAuxMetaData> Package::auxMetaDataFile(std::string_view mediaType, size_t index) const
{
    if (!m_impl)
    {
        return Error(ErrorCode::InvalidPackage, "Invalid Package object");
    }

    Error err;
    auto auxFile = m_impl->auxMetaDataFile(mediaType, index, &err);
    if (!auxFile || err)
    {
        return err;
    }

    return PackageAuxMetaData(std::move(auxFile));
}

Result<size_t> Package::auxMetaDataFileCount(std::string_view mediaType) const
{
    if (!m_impl)
    {
        return Error(ErrorCode::InvalidPackage, "Invalid Package object");
    }

    return m_impl->auxMetaDataFileCount(mediaType);
}

Result<std::set<std::string>> Package::auxMetaDataKeys() const
{
    if (!m_impl)
    {
        return Error(ErrorCode::InvalidPackage, "Invalid Package object");
    }

    return m_impl->auxMetaDataKeys();
}

size_t Package::maxExtractionBytes() const
{
    if (!m_impl)
        return SIZE_MAX;
    else
        return m_impl->maxExtractionBytes();
}

void Package::setMaxExtractionBytes(size_t maxTotalSize)
{
    if (m_impl)
        m_impl->setMaxExtractionBytes(maxTotalSize);
}

size_t Package::maxExtractionEntries() const
{
    if (!m_impl)
        return SIZE_MAX;
    else
        return m_impl->maxExtractionEntries();
}

void Package::setMaxExtractionEntries(size_t maxFileCount)
{
    if (m_impl)
        m_impl->setMaxExtractionEntries(maxFileCount);
}

std::ostream &LIBRALF_NS::operator<<(std::ostream &s, const Package::Format &format)
{
    switch (format)
    {
        case Package::Format::Unknown:
            s << "Package::Format::Unknown";
            break;
        case Package::Format::Widget:
            s << "Package::Format::Widget";
            break;
        case Package::Format::Ralf:
            s << "Package::Format::Ralf";
            break;
    }

    return s;
}
