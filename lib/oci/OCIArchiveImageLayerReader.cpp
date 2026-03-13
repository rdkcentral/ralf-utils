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

#include "OCIArchiveImageLayerReader.h"
#include "IOCIBackingStore.h"
#include "OCIDescriptor.h"
#include "OCIMediaTypes.h"
#include "OCIUtils.h"
#include "archive/LibarchiveStreamReader.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

/// The maximum size of an OCI image layer is 512MB (FIXME: add as property you can set on the library)
#define MAX_IMAGE_SIZE (512ULL * 1024 * 1024)

OCIArchiveImageLayerReader::OCIArchiveImageLayerReader(std::shared_ptr<const OCIDescriptor> imageDescriptor,
                                                       std::unique_ptr<IOCIFileReader> &&imageReader)
    : m_imageDescriptor(std::move(imageDescriptor))
    , m_imageReader(std::move(imageReader))
    , m_imageSize(static_cast<int64_t>(m_imageDescriptor->size()))
    , m_imageOffset(0)
    , m_digestBuilder(CryptoDigestBuilder::Algorithm::Sha256)
    , m_buffer(8192)
{

    // Sanity check the size of the image
    if (m_imageDescriptor->size() > MAX_IMAGE_SIZE)
    {
        setError(ErrorCode::PackageContentsInvalid, "OCI image layer is too large (%" PRIu64 ")", m_imageSize);
        return;
    }

    ArchiveFormats requiredFormats;
    if (m_imageDescriptor->mediaType() == PACKAGE_IMAGE_MEDIA_TYPE_PACKAGE_CONTENT_TAR)
        requiredFormats = ArchiveFormats::Tarball;
    else if (m_imageDescriptor->mediaType() == PACKAGE_IMAGE_MEDIA_TYPE_PACKAGE_CONTENT_TAR_GZIP)
        requiredFormats = ArchiveFormats::TarballGzip;
    else if (m_imageDescriptor->mediaType() == PACKAGE_IMAGE_MEDIA_TYPE_PACKAGE_CONTENT_TAR_ZSTD)
        requiredFormats = ArchiveFormats::TarballZstd;
    else if (m_imageDescriptor->mediaType() == PACKAGE_IMAGE_MEDIA_TYPE_PACKAGE_CONTENT_ZIP)
        requiredFormats = ArchiveFormats::Zip;
    else
    {
        setError(ErrorCode::PackageContentsInvalid, "Unsupported OCI image layer media type '%s'",
                 m_imageDescriptor->mediaType().c_str());
        return;
    }

    Error error;

    // Create an archive stream reader with our own read and skip callbacks, the callbacks read from the imageReader
    // object, which is the backing store for the OCI image layer.  As the bytes are read from the image they are passed
    // to the digest builder to calculate the digest over the entire image layer.
    // clang-format off
    auto streamReader = std::make_unique<LibarchiveStreamReader>(
        [this](const void *_Nonnull *_Nonnull buffer) { return readCallback(buffer); },
        [this](int64_t skip) { return skipCallback(skip); },
        requiredFormats, &error);
    // clang-format on

    if (!streamReader || streamReader->isNull())
    {
        setError(ErrorCode::PackageContentsInvalid, "Failed to create archive stream reader - %s", error.what());
        return;
    }

    // Now wrap the low level archive stream reader with a higher level archive reader that confirms to the
    // IPackageReaderImpl interface, this is the object that will be used to read the archive entries.
    // Note we don't pass a file digest map to the object, as in OCI the digest is calculated over the entire
    // archive, not per file in the archive.  However, the reader callbacks will calculate the digest over the
    // entire image layer as it reads the bytes, so we can compare the final digest to the expected value in the
    // descriptor.
    m_archiveReader = std::make_unique<ArchiveReader>(std::move(streamReader));
}

OCIArchiveImageLayerReader::~OCIArchiveImageLayerReader() // NOLINT(modernize-use-equals-default)
{
}

// -------------------------------------------------------------------------
/*!
    \internal

    Read callback for the archive reader.  This is responsible for reading
    bytes of the OCI image layer from the backing store.  This also updates
    the digest value as it reads the bytes, at the end the digest is compared
    to the expected value in the descriptor.

 */
ssize_t OCIArchiveImageLayerReader::readCallback(const void *_Nonnull *_Nonnull buffer)
{
    if (m_imageOffset >= m_imageSize)
        return 0;

    const ssize_t amount =
        std::min<ssize_t>(static_cast<ssize_t>(m_imageSize - m_imageOffset), static_cast<ssize_t>(m_buffer.size()));

    const ssize_t rd = m_imageReader->read(m_buffer.data(), amount);
    if (rd <= 0)
    {
        setError(ErrorCode::PackageContentsInvalid, "Failed to read from OCI image layer");
        return -1;
    }

    m_digestBuilder.update(m_buffer.data(), rd);
    m_imageOffset += rd;

    *buffer = m_buffer.data();
    return rd;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Skip callback for the archive reader.  This is basically the same as
    readCallback, because we still have to read the bytes from the image to
    calculate the digest over them.  However this is a slight optimisation
    because we don't have to return the bytes to libarchive for processing.

 */
int64_t OCIArchiveImageLayerReader::skipCallback(int64_t skip)
{
    if (m_imageOffset >= m_imageSize)
        return 0;

    skip = std::min(skip, m_imageSize - m_imageOffset);
    int64_t skipped = 0;

    uint8_t buffer[4096];

    while (skipped < skip)
    {
        const size_t amount = std::min<size_t>(sizeof(buffer), (skip - skipped));

        ssize_t rd = m_imageReader->read(buffer, amount);
        if (rd <= 0)
        {
            setError(ErrorCode::PackageContentsInvalid, "Failed to read from OCI image layer");
            return -1;
        }

        m_digestBuilder.update(buffer, rd);
        m_imageOffset += rd;

        skipped += rd;
    }

    return skipped;
}

// -------------------------------------------------------------------------
/*!
    Retrieves the next entry from the archive.  This is mostly just a wrapper
    around the ArchiveReader::next() method, but it also checks the digest
    once the end of the archive is reached.  If the digest doesn't match
    then an error is set and nullptr is returned.

    The obvious problem with this logic, is that if the user decides to stop
    reading the archive before the end, then the digest will not be checked.
    There is not much we can do about this, and just have to make it clear
    that users of this API must read the entire archive to ensure it has not
    been tampered with.  This is the same as the OCI image spec, which
    states that the digest is over the entire image layer, not just the
    files in the image layer.

    We expect we'll want use fs-verity or dm-verity to verify the image
    layer in the future.

 */
std::unique_ptr<IPackageEntryImpl> OCIArchiveImageLayerReader::next()
{
    if (hasError())
        return nullptr;

    auto entry = m_archiveReader->next();
    if (m_archiveReader->hasError())
    {
        m_error = m_archiveReader->error();
        return nullptr;
    }

    // If failed to get the next entry, and there is no error, it means we've reached the end of the archive.
    // However, we may not have reached the end of the image (ie. libarchive could have skipped the tar 1024 byte
    // trailer), so ensure we read all the way to the end of the image so the digest is calculated over the entire
    // image.
    if (!entry)
    {
        // Read any remaining bytes from the image layer, this will also update the digest.
        if (m_imageOffset < m_imageSize)
            skipCallback(m_imageSize - m_imageOffset);

        // Now check the digest against the expected value in the descriptor.
        const auto expectedDigest = hexStringToBytes(m_imageDescriptor->digest());
        const auto actualDigest = m_digestBuilder.finalise();
        if (actualDigest != expectedDigest)
        {
            setError(ErrorCode::InvalidPackage, "Digest mismatch for OCI image layer");
            return nullptr;
        }
    }

    return entry;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Sets the error state with the given error code and formatted message.
 */
void OCIArchiveImageLayerReader::setError(LIBRALF_NS::ErrorCode code, const char *format, ...)
{
    std::va_list args;
    va_start(args, format);
    m_error = Error::format(code, format, args);
    va_end(args);
}

// -------------------------------------------------------------------------
/*!
    Returns true if an error has occurred while reading the archive or
    if reached the end of the archive but the digest didn't match.

 */
bool OCIArchiveImageLayerReader::hasError() const
{
    return m_error.operator bool();
}

// -------------------------------------------------------------------------
/*!
    Returns the error object if an error has occurred while reading the
    archive or if the digest didn't match.

 */
Error OCIArchiveImageLayerReader::error() const
{
    return m_error;
}
