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

#include "archive/ArchiveReader.h"
#include "core/CryptoDigestBuilder.h"
#include "core/IPackageReaderImpl.h"

#include <memory>

class OCIDescriptor;
class IOCIFileReader;

// -------------------------------------------------------------------------
/*!
    \class OCIArchiveImageLayerReader
    \brief Implement of a PackageReader that reads from an OCI archive image
    layer.

    For example if the OCI package has a layer with type tar+gzip, or zip then
    this is the reader that will be used to read the contents of the layer.

    This code re-uses the ArchiveReader code to read the contents of the
    archive, however it doesn't pass a digest map, because for OCI packages
    the digest of over the archive file, not the individual digests per file
    in the archive.  This means that we use a streaming archive reader to read
    the archive bytes and calculate the digests as we go and process the archive.

*/
class OCIArchiveImageLayerReader final : public LIBRALF_NS::IPackageReaderImpl
{
public:
    OCIArchiveImageLayerReader(std::shared_ptr<const OCIDescriptor> imageDescriptor,
                               std::unique_ptr<IOCIFileReader> &&imageReader);
    ~OCIArchiveImageLayerReader() final;

    std::unique_ptr<LIBRALF_NS::IPackageEntryImpl> next() override;

    bool hasError() const override;

    LIBRALF_NS::Error error() const override;

private:
    ssize_t readCallback(const void *_Nonnull *_Nonnull buffer);
    int64_t skipCallback(int64_t skip);

    void setError(LIBRALF_NS::ErrorCode code, const char *_Nonnull format, ...) __attribute__((format(printf, 3, 4)));

private:
    const std::shared_ptr<const OCIDescriptor> m_imageDescriptor;
    const std::unique_ptr<IOCIFileReader> m_imageReader;
    const int64_t m_imageSize;
    int64_t m_imageOffset;

    LIBRALF_NS::CryptoDigestBuilder m_digestBuilder;

    std::vector<uint8_t> m_buffer;
    std::unique_ptr<entos::ralf::archive::ArchiveReader> m_archiveReader;

    LIBRALF_NS::Error m_error;
};
