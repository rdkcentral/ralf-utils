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

#include "W3CPackage.h"
#include "W3CPackageMetaDataBuilder.h"
#include "ZipReaderFactory.h"
#include "archive/ArchiveReader.h"
#include "archive/LibarchiveFileReader.h"
#include "core/CryptoDigestBuilder.h"
#include "core/LogMacros.h"
#include "core/Utils.h"
#include "crypto/xmldsig/XmlDigitalSignature.h"

#include <archive.h>
#include <archive_entry.h>

#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

/// The name of the xml digital signature file within the package, this is fixed.
#define SIGNATURE_FILE_NAME "signature1.xml"

/// The maximum size allowed for the signature1.xml file, since we load the signature file into memory, we need to
/// limit the size to prevent DoS attacks.
#define MAX_SIGNATURE_SIZE (8 * 1024 * 1024)

/// The maximum size allowed for meta-data files, since we load the meta-data files into memory for processing, we need
/// to limit the size to prevent DoS attacks.
#define MAX_METADATA_FILE_SIZE (4 * 1024 * 1024)

/// The maximum size allowed for a entry within a package.  This is to prevent out of memory errors if the package
/// contains malicious data.
#define MAX_PACKAGE_ENTRY_SIZE (128 * 1024 * 1024)

// -------------------------------------------------------------------------
/*!
    Attempts to open the package with initial basic signature check.

 */
std::unique_ptr<W3CPackage> W3CPackage::open(int packageFd, const std::optional<VerificationBundle> &bundle,
                                             Package::OpenFlags flags, Error *_Nullable error)
{
    return open(packageFd, std::make_shared<ZipReaderFactory>(), bundle, flags, error);
}

// -------------------------------------------------------------------------
/*!
    Attempts to open the package with optional verification, using the
    supplied archive reader factory to create the archive reader objects.

*/
std::unique_ptr<W3CPackage> W3CPackage::open(int packageFd, std::shared_ptr<IZipReaderFactory> zipReaderFactory,
                                             const std::optional<LIBRALF_NS::VerificationBundle> &bundle,
                                             LIBRALF_NS::Package::OpenFlags flags, LIBRALF_NS::Error *_Nullable error)
{
    // Create a wrapper on the package
    auto package = std::unique_ptr<W3CPackage>(new W3CPackage(packageFd, std::move(zipReaderFactory), error));
    if (!package || !package->isValid())
        return nullptr;

    // If a bundle was supplied then verify the signature of the package, this doesn't check the digests
    // of the files in the package, it just verifies the signature1.xml file.  When the files are read
    // they'll have their digests checked ... or if verify() is called.
    if (bundle)
    {
        if (!package->readAndVerifySignature(bundle.value(), flags, error))
            return nullptr;
    }

    return package;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Constructs the package object by dup'ing the file descriptor and storing
    the size of the package.
*/
W3CPackage::W3CPackage(int packageFd, std::shared_ptr<IZipReaderFactory> &&zipReaderFactory,
                       LIBRALF_NS::Error *_Nullable error)
    : m_signatureFileName(SIGNATURE_FILE_NAME)
    , m_zipReaderFactory(std::move(zipReaderFactory))
    , m_packageFd(-1)
{
    // dup the file descriptor so we have our own copy
    int duppedFd = fcntl(packageFd, F_DUPFD_CLOEXEC, 3);
    if (duppedFd < 0)
    {
        if (error)
            error->assign(std::error_code(errno, std::system_category()), "Failed to dup package fd");
        return;
    }

    struct stat st = {};
    if (fstat(duppedFd, &st) < 0)
    {
        if (error)
            error->assign(std::error_code(errno, std::system_category()), "Failed to get package size");

        close(duppedFd);
        return;
    }

    m_packageSize = st.st_size;
    m_packageFd = duppedFd;
}

W3CPackage::~W3CPackage()
{
    if (m_packageFd >= 0)
        close(m_packageFd);
}

// -------------------------------------------------------------------------
/*!
    \internal
    \static

    Helper function to read the entire contents of the file in the archive
    and returns it as buffer.

*/
std::optional<std::vector<uint8_t>> W3CPackage::readArchiveFileEntry(const std::unique_ptr<ILibarchiveReader> &archive,
                                                                     const size_t size, Error *_Nullable error)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(size);

    char buf[4096];
    ssize_t len = archive->readData(buf, sizeof(buf));
    while (len > 0)
    {
        if ((buffer.size() + len) > size)
        {
            if (error)
                error->assign(ErrorCode::PackageFileTooLarge, "File is too large");

            return std::nullopt;
        }

        buffer.insert(buffer.end(), buf, buf + len);
        len = archive->readData(buf, sizeof(buf));
    }

    return buffer;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Attempts to read the signature1.xml file from the package and verify it
    using the supplied verification bundle.  If successful the digests of the
    files in the package are stored internally for later verification.

*/
bool W3CPackage::readAndVerifySignature(const VerificationBundle &bundle, Package::OpenFlags flags, Error *_Nullable error)
{
    std::unique_lock locker(m_cachedDataLock);

    // Clear any existing verification results
    m_digests.reset();
    m_signingCerts.clear();

    locker.unlock();

    // Create an archive reader
    auto result = m_zipReaderFactory->createReader(m_packageFd, m_packageSize, false);
    if (!result)
    {
        if (error)
            *error = result.error();

        return false;
    }

    auto archive = std::move(result.value());

    std::vector<uint8_t> signature1Xml;

    // Attempt to read the signature1.xml file
    archive_entry *entry = nullptr;
    while (archive->nextHeader(&entry) == ARCHIVE_OK)
    {
        const char *entryPath = archive_entry_pathname(entry);
        const mode_t type = (archive_entry_filetype(entry) & AE_IFMT);

        if ((type == AE_IFREG) && entryPath && (strcmp(entryPath, SIGNATURE_FILE_NAME) == 0))
        {
            const auto size = archive_entry_size(entry);
            if ((size < 0) || (size > MAX_SIGNATURE_SIZE))
            {
                if (error)
                    error->assign(ErrorCode::PackageSignatureInvalid, SIGNATURE_FILE_NAME " file is too large");
                return false;
            }

            auto contents = readArchiveFileEntry(archive, static_cast<size_t>(size), error);
            if (!contents)
            {
                return false;
            }

            signature1Xml = std::move(contents.value());
            break;
        }
    }

    // Can drop the archive reader now
    archive.reset();

    // If we didn't find the signature1.xml file then we can't verify the package
    if (signature1Xml.empty())
    {
        if (error)
            error->assign(ErrorCode::PackageSignatureMissing, "Failed to find " SIGNATURE_FILE_NAME " file in package");

        return false;
    }

    // Parse the xmldsig file, this step doesn't do the cryptographic verification, it just does the sanity
    // checks on the xml document.
    XmlDigitalSignature signature = XmlDigitalSignature::parse(signature1Xml, error);
    if (signature.isNull())
    {
        // Error object already set
        return false;
    }

    // If the CheckCertificateExpiry flag is set then we check the expiry time of the certificates
    XmlDigitalSignature::VerifyOptions options = XmlDigitalSignature::VerifyOption::None;
    if ((flags & Package::OpenFlag::CheckCertificateExpiry) == Package::OpenFlag::CheckCertificateExpiry)
        options |= XmlDigitalSignature::VerifyOption::CheckCertificateExpiry;

    // Verify the signature1.xml file
    if (!signature.verify(bundle, options, error))
    {
        // Error object already set
        return false;
    }

    // Signature verified so get all the references to the files and their (verified) digests
    auto references = signature.references(error);
    if (references.empty())
    {
        // Error object already set
        return false;
    }

    // The XmlDigitalSignature object doesn't do any sanity check on the reference paths, so we need to do it here.
    // In particular, we need to check that the paths are not absolute, or contain "..".  We also need to double
    // check the crypt digest algorithm used is sha56.
    auto digests = std::make_shared<FileDigestMap>();
    for (auto &reference : references)
    {
        // Check the path is not absolute and doesn't contain ".."
        std::filesystem::path path = std::filesystem::path(reference.uri).lexically_normal();
        if (path.is_absolute() || path.empty() || (*path.begin() == ".."))
        {
            if (error)
                *error = Error::format(ErrorCode::PackageSignatureInvalid, "Invalid path '%s' in " SIGNATURE_FILE_NAME,
                                       reference.uri.c_str());

            return false;
        }

        // Check the digest algorithm is sha256 and the size of the digest is correct
        if ((reference.digestAlgorithm != XmlDigitalSignature::DigestAlgorithm::Sha256) ||
            (reference.digestValue.size() != 32))
        {
            if (error)
                *error = Error::format(ErrorCode::PackageSignatureInvalid,
                                       "Invalid digest algorithm or size for '%s' in " SIGNATURE_FILE_NAME,
                                       reference.uri.c_str());

            return false;
        }

        // Store the digest for later verification
        digests->emplace(std::move(path), std::move(reference.digestValue));
    }

    // Also add the digest of the signature to the map, this just removes the special case handling for the
    // signature1.xml file when reading the archive.
    digests->emplace(m_signatureFileName,
                     CryptoDigestBuilder::digest(CryptoDigestBuilder::Algorithm::Sha256, signature1Xml));

    // Take the lock before updating the cached data
    locker.lock();

    // Store the digests internally for later verification
    m_digests = std::move(digests);

    // Store the signing certs in case requested later
    m_signingCerts = signature.signingCertificates();

    return true;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Calculates the digest of the \a fileContents and compares it to the
    verified map of package digests stored internally.

*/
bool W3CPackage::verifyFileContents(const std::filesystem::path &filePath, const std::vector<uint8_t> &fileContents,
                                    Error *_Nullable error)
{
    // If no digests then assume we're skipping verification, may seem counter-intuitive but
    // the rest of the code needs to verify that the digests exist and are correct.
    if (!m_digests)
    {
        return true;
    }

    // Find the digest for the file
    auto it = m_digests->find(filePath);
    if (it == m_digests->end())
    {
        if (error)
        {
            *error = Error::format(ErrorCode::PackageContentsInvalid, "No signed digest found for file '%s'",
                                   filePath.c_str());
        }

        return false;
    }

    // Calculate the SHA256 digest of the file contents
    const auto actualDigest = CryptoDigestBuilder::digest(CryptoDigestBuilder::Algorithm::Sha256, fileContents);
    const auto &expectedDigest = it->second;

    // Compare the digests
    if (actualDigest != expectedDigest)
    {
        if (error)
        {
            *error = Error::format(ErrorCode::PackageContentsInvalid, "Digest of '%s' does not match signed digest",
                                   filePath.c_str());
        }

        return false;
    }

    return true;
}

bool W3CPackage::isValid() const
{
    return (m_packageFd >= 0);
}

bool W3CPackage::isMountable() const
{
    return false;
}

Package::Format W3CPackage::format() const
{
    return Package::Format::Widget;
}

ssize_t W3CPackage::size() const
{
    return static_cast<ssize_t>(m_packageSize);
}

ssize_t W3CPackage::unpackedSize() const
{
    // If already calculated then return it
    if (m_unpackedSize.load() >= 0)
        return m_unpackedSize;

    // Create an archive reader
    auto result = m_zipReaderFactory->createReader(m_packageFd, m_packageSize, false);
    if (!result)
        return -1;

    auto archive = std::move(result.value());

    ssize_t totalSize = 0;

    // Loop through every entry getting the unpacked size
    archive_entry *entry = nullptr;
    while (archive->nextHeader(&entry) == ARCHIVE_OK)
    {
        const mode_t type = (archive_entry_filetype(entry) & AE_IFMT);
        if (type == AE_IFREG)
        {
            const auto size = archive_entry_size(entry);
            if (size > 0)
                totalSize += static_cast<ssize_t>(size);
        }
    }

    m_unpackedSize.store(totalSize);
    return totalSize;
}

// -------------------------------------------------------------------------
/*!
    \internal
    \static

    Verifies the contents of the current file entry in the archive against
    the SHA256 \a digest.

    Returns \c true if the contents of the file match the digest, otherwise
    \c false.  If \a error is supplied then it will be populated with the
    reason for the failure.
 */
bool W3CPackage::verifyArchiveFileEntry(const std::unique_ptr<ILibarchiveReader> &archive, const size_t expectedSize,
                                        const std::vector<uint8_t> &digest, Error *_Nullable error)
{
    // Create a digest builder for verifying the file contents
    CryptoDigestBuilder digestBuilder(CryptoDigestBuilder::Algorithm::Sha256);

    const void *blockPtr = nullptr;
    int64_t blockOffset;
    size_t blockSize;
    int64_t currentOffset = 0;

    // Read the contents of the file and calculate the digest
    int rc;
    while ((rc = archive->readDataBlock(&blockPtr, &blockSize, &blockOffset)) == ARCHIVE_OK)
    {
        if (blockSize == 0)
            continue;

        // Sanity check the entry is within
        if ((blockSize > MAX_PACKAGE_ENTRY_SIZE) || ((blockOffset + blockSize) > MAX_PACKAGE_ENTRY_SIZE))
        {
            if (error)
                error->assign(ErrorCode::PackageContentsInvalid, "Invalid size of entry in package");
            return false;
        }

        // Sanity check the offset hasn't gone backwards
        if (blockOffset < currentOffset)
        {
            if (error)
                error->assign(ErrorCode::PackageContentsInvalid, "Invalid block offset in package");
            return false;
        }

        // Check if we need zero padding to the digest calculation
        if (blockOffset > currentOffset)
        {
            digestBuilder.update(nullptr, static_cast<size_t>(blockOffset - currentOffset));
        }

        // Update the digest with the block data
        digestBuilder.update(blockPtr, blockSize);

        // Update to the end of the block
        currentOffset = blockOffset + static_cast<int64_t>(blockSize);

        // Sanity check the size of the entry
        if (currentOffset > static_cast<int64_t>(expectedSize))
        {
            if (error)
                error->assign(ErrorCode::PackageContentsInvalid, "Invalid size of entry in package");
            return false;
        }
    }

    // Check we actually read the entire entry
    if (rc != ARCHIVE_EOF)
    {
        if (error)
            error->assign(ErrorCode::PackageContentsInvalid, "Failed to read an entry in the package");
        return false;
    }

    // Finally check that the calculated digest matches the signed digest
    if (digest != digestBuilder.finalise())
    {
        if (error)
            error->assign(ErrorCode::PackageContentsInvalid, "Digest of entry in package does not match signed digest");
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    Iterates through the files in the archive and verifies the digests of each
    against the values in the file digests map.

 */
bool W3CPackage::verify(Error *_Nullable error)
{
    if (!m_digests)
    {
        if (error)
            error->assign(ErrorCode::NotSupported, "Package wasn't opened with a verification bundle");
        return false;
    }

    // Check if we have too many files in the package
    if (m_digests->size() > m_maxExtractionEntries)
    {
        if (error)
            error->assign(ErrorCode::PackageContentsInvalid, "Package contains too many files");
        return false;
    }

    // Open the archive reader
    auto result = m_zipReaderFactory->createReader(m_packageFd, m_packageSize, false);
    if (!result)
    {
        if (error)
            *error = result.error();
        return false;
    }

    auto archive = std::move(result.value());

    // The total number of bytes unpacked
    uint64_t totalSize = 0;

    // Counter of the number of files verified
    size_t entriesVerified = 0;

    // Loop through every entry getting the unpacked size
    archive_entry *entry = nullptr;
    while (archive->nextHeader(&entry) == ARCHIVE_OK)
    {
        // Get the path of the entry
        const char *entryPath = archive_entry_pathname(entry);
        if (!entryPath)
        {
            if (error)
                error->assign(ErrorCode::PackageContentsInvalid, "Invalid entry in package");
            return false;
        }

        // Sanity check the path
        const std::filesystem::path path = std::filesystem::path(entryPath).lexically_normal();
        if (!verifyPackagePath(path))
        {
            if (error)
                error->assign(ErrorCode::PackageContentsInvalid, "Invalid path entry in package");
            return false;
        }

        // Get the type and skip directories
        const mode_t type = (archive_entry_filetype(entry) & AE_IFMT);
        if (type == AE_IFDIR)
            continue;

        // Find the digest for the file
        auto it = m_digests->find(path);
        if (it == m_digests->end())
        {
            if (error)
                *error = Error::format(ErrorCode::PackageContentsInvalid, "No signed digest found for file '%s'",
                                       path.c_str());
            return false;
        }

        // If a regular file then read the contents and calculate the digest
        if (type == AE_IFREG)
        {
            const auto size = archive_entry_size(entry);
            if ((size < 0) || (size > MAX_PACKAGE_ENTRY_SIZE))
            {
                if (error)
                    *error = Error::format(ErrorCode::PackageContentsInvalid, "Invalid size of entry '%s' in package",
                                           path.c_str());
                return false;
            }

            totalSize += size;
            if (totalSize > static_cast<uint64_t>(m_maxExtractionBytes))
            {
                if (error)
                    *error = Error::format(ErrorCode::PackageContentsInvalid,
                                           "Total unpacked size of package is too large (size:%" PRId64 ", limit:%zu)",
                                           totalSize, m_maxExtractionBytes);
                return false;
            }

            if (!verifyArchiveFileEntry(archive, size, it->second, error))
            {
                return false;
            }
        }
        else if (type == AE_IFLNK)
        {
            const char *target = archive_entry_symlink_utf8(entry);
            if (!target)
            {
                if (error)
                    *error =
                        Error::format(ErrorCode::PackageContentsInvalid, "Invalid symlink target for '%s'", entryPath);
                return false;
            }

            if (CryptoDigestBuilder::digest(CryptoDigestBuilder::Algorithm::Sha256, target, strlen(target)) != it->second)
            {
                if (error)
                    *error = Error::format(ErrorCode::PackageContentsInvalid,
                                           "Digest of symlink target for '%s' does not match signed digest", entryPath);
                return false;
            }
        }
        else
        {
            if (error)
                *error = Error::format(ErrorCode::PackageContentsInvalid,
                                       "Package contains entry with invalid type '%s'", path.c_str());
            return false;
        }

        entriesVerified++;
    }

    // Check that all files in the digest map were found in the package
    if (entriesVerified != m_digests->size())
    {
        if (error)
            *error = Error::format(ErrorCode::PackageContentsInvalid, "Package was missing some signed file(s)");
        return false;
    }

    // Success
    return true;
}

// -------------------------------------------------------------------------
/*!
    Mount is not support for zip packages.

*/
Result<std::unique_ptr<IPackageMountImpl>> W3CPackage::mount(const std::filesystem::path &mountPoint, MountFlags flags)
{
    (void)mountPoint;
    (void)flags;

    // There is no support for mounting zip packages
    return Error(ErrorCode::NotSupported, "Cannot mount widget / zip packages");
}

// -------------------------------------------------------------------------
/*!
    \internal

    \warning This method should be called with the m_cachedDataLock held.

    Reads the meta-data from the package with verification if a package was
    opened with a verification bundle.

*/
std::shared_ptr<W3CPackageMetaDataImpl> W3CPackage::readMetaData(Error *_Nullable error)
{
    // Create an archive reader
    auto result = m_zipReaderFactory->createReader(m_packageFd, m_packageSize, false);
    if (!result)
    {
        if (error)
            *error = result.error();
        return nullptr;
    }

    auto archive = std::move(result.value());

    // Create a builder object for the metadata
    W3CPackageMetaDataBuilder builder;

    // Attempt to read the meta-data file(s) from the package
    archive_entry *entry = nullptr;
    while (archive->nextHeader(&entry) == ARCHIVE_OK)
    {
        const char *entryPath = archive_entry_pathname(entry);
        const mode_t type = (archive_entry_filetype(entry) & AE_IFMT);

        if ((type == AE_IFREG) && entryPath)
        {
            // Skip if the file is not a meta-data file
            const std::filesystem::path filePath = std::filesystem::path(entryPath).lexically_normal();
            if (!builder.isMetaDataFile(filePath))
            {
                continue;
            }

            // Check the size of the entry
            const auto size = archive_entry_size(entry);
            if ((size < 0) || (size > MAX_METADATA_FILE_SIZE))
            {
                if (error)
                    *error = Error::format(ErrorCode::PackageContentsInvalid, "'%s' file is too large", entryPath);

                return {};
            }

            // Read the contents of the file
            auto contents = readArchiveFileEntry(archive, static_cast<size_t>(size), error);
            if (!contents)
            {
                // Error already set
                return {};
            }

            // Verify the file if we have a verification bundle
            if (!verifyFileContents(filePath, contents.value(), error))
            {
                // Error already set
                return {};
            }

            // Add the meta-data file to the builder
            builder.addMetaDataFile(entryPath, std::move(contents.value()));
        }
    }

    // Can drop the archive reader now
    archive.reset();

    // Build the meta-data object
    return builder.generate(error);
}

// -------------------------------------------------------------------------
/*!
    Returns meta-data read from the package.

    If the package was opened with a verification bundle then meta data will
    have been verified.
 */
std::shared_ptr<IPackageMetaDataImpl> W3CPackage::metaData(Error *_Nullable error)
{
    if (error)
        error->clear();

    std::lock_guard locker(m_cachedDataLock);

    if (!m_metaData)
    {
        // Haven't yet tried to read the metadata, so do it now
        m_metaData = readMetaData(error);
    }

    return m_metaData;
}

// -------------------------------------------------------------------------
/*!
    Returns the signing certificates from signature1.xml file in the package.

    If the package was opened with a verification bundle then the certificates
    will have been verified.

 */
std::list<Certificate> W3CPackage::signingCertificates(Error *_Nullable error)
{
    if (error)
        error->clear();

    std::lock_guard locker(m_cachedDataLock);

    return m_signingCerts;
}

// -------------------------------------------------------------------------
/*!
    Creates a new reader object for this package.  A reader is just a
    wrapper around a libarchive reader, with an optional sha256 check of the
    entry contents.

    If the package was opened with a verification bundle then the reader will
    get a reference to the digests of the files in the package and the reader
    will verify the digests as the files are read.

 */
std::unique_ptr<IPackageReaderImpl> W3CPackage::createReader(Error *_Nullable error)
{
    if (error)
        error->clear();

    // dup the package fd so the reader has its own copy, and it can outlive this object
    int duppedFd = fcntl(m_packageFd, F_DUPFD_CLOEXEC, 3);
    if (duppedFd < 0)
    {
        if (error)
            error->assign(std::error_code(errno, std::system_category()), "Failed to dup package fd");

        return nullptr;
    }

    // Create an archive reader, it will take ownership of the fd
    auto result = m_zipReaderFactory->createReader(duppedFd, m_packageSize, true);
    if (!result)
    {
        if (error)
            *error = result.error();
        return nullptr;
    }

    // Gift the archive reader and the optional digests to the reader object
    return std::make_unique<ArchiveReader>(std::move(result.value()), m_digests);
}

// -------------------------------------------------------------------------
/*!
    For traditional packages we treat the appsecrets.json file as some
    auxiliary meta-data, so we return the contents of that file if it
    exists.

 */
std::unique_ptr<IPackageAuxMetaDataImpl> W3CPackage::auxMetaDataFile(std::string_view mediaType, size_t index,
                                                                     LIBRALF_NS::Error *_Nullable error)
{
    if (index != 0)
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidArgument, "No auxiliary meta-data file with index %zu", index);
        return nullptr;
    }

    // Get the meta-data from the package, for W3C packages the auxiliary meta-data is parsed with the main meta-data
    std::unique_lock locker(m_cachedDataLock);

    if (!m_metaData)
    {
        m_metaData = readMetaData(error);
        if (!m_metaData)
            return nullptr;
    }

    locker.unlock();

    auto auxMetaData = m_metaData->getAuxMetaDataFile(mediaType);
    if (auxMetaData)
    {
        return std::move(auxMetaData.value());
    }

    if (error)
    {
        *error = auxMetaData.error();
    }

    return nullptr;
}

// -------------------------------------------------------------------------
/*!
    Checks if the \a mediaType corresponds to a request for appsecrets.json
    (the only auxiliary meta-data file supported by W3C packages), and then
    checks if the file exists in the package.

*/
Result<size_t> W3CPackage::auxMetaDataFileCount(std::string_view mediaType)
{
    // Get the meta-data from the package, for W3C packages the auxiliary meta-data is parsed with the main meta-data
    std::unique_lock locker(m_cachedDataLock);

    if (!m_metaData)
    {
        Error err;
        m_metaData = readMetaData(&err);
        if (!m_metaData || err)
            return err;
    }

    locker.unlock();

    return m_metaData->hasAuxMetaDataFile(mediaType) ? 1 : 0;
}

Result<std::set<std::string>> W3CPackage::auxMetaDataKeys()
{
    // Get the meta-data from the package, for W3C packages the auxiliary meta-data is parsed with the main meta-data
    std::unique_lock locker(m_cachedDataLock);

    if (!m_metaData)
    {
        Error err;
        m_metaData = readMetaData(&err);
        if (!m_metaData || err)
            return err;
    }

    locker.unlock();

    return m_metaData->availAuxMetaData();
}

size_t W3CPackage::maxExtractionBytes() const
{
    return m_maxExtractionBytes;
}

void W3CPackage::setMaxExtractionBytes(size_t maxTotalSize)
{
    m_maxExtractionBytes = maxTotalSize;
}

size_t W3CPackage::maxExtractionEntries() const
{
    return m_maxExtractionEntries;
}

void W3CPackage::setMaxExtractionEntries(size_t maxFileCount)
{
    m_maxExtractionEntries = maxFileCount;
}
