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

#include "core/IPackageImpl.h"

#include <atomic>
#include <climits>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace entos::ralf::archive
{
    class ILibarchiveReader;
}

class IZipReaderFactory;
class W3CPackageMetaDataImpl;

// -------------------------------------------------------------------------
/*!
    \class W3CPackage
    \brief Implement of a package that uses the W3C widget format (aka zip fle).

    "W3C Widgets" is a specification that defines a packaging format for web
    apps, it is largely deprecated, but it served as the basis of app packages
    on EntOS platforms.  The package is a zip file that must contain a config.xml
    file and a signature1.xml file.

    The config.xml is EntOS specific but loosely based on the W3C widget
    config.xml file.  It contains things like the app id, title, version, etc.
    It also contains the capabilities of the app and is used as the source
    for PackageMetaData.

    The signature1.xml file is an XMLDSig signature of the entire contents
    of the package, it is used to verify the integrity of the package.

     */
class W3CPackage final : public LIBRALF_NS::IPackageImpl
{
public:
    static std::unique_ptr<W3CPackage> open(int packageFd, const std::optional<LIBRALF_NS::VerificationBundle> &bundle,
                                            LIBRALF_NS::Package::OpenFlags flags, LIBRALF_NS::Error *_Nullable error);

    static std::unique_ptr<W3CPackage> open(int packageFd, std::shared_ptr<IZipReaderFactory> zipReaderFactory,
                                            const std::optional<LIBRALF_NS::VerificationBundle> &bundle,
                                            LIBRALF_NS::Package::OpenFlags flags, LIBRALF_NS::Error *_Nullable error);

public:
    ~W3CPackage() final;

    bool isValid() const;

    bool isMountable() const override;

    LIBRALF_NS::Package::Format format() const override;

    ssize_t size() const override;

    ssize_t unpackedSize() const override;

    bool verify(LIBRALF_NS::Error *_Nullable error) override;

    LIBRALF_NS::Result<std::unique_ptr<LIBRALF_NS::IPackageMountImpl>> mount(const std::filesystem::path &mountPoint,
                                                                             LIBRALF_NS::MountFlags flags) override;

    std::shared_ptr<LIBRALF_NS::IPackageMetaDataImpl> metaData(LIBRALF_NS::Error *_Nullable error) override;

    std::list<LIBRALF_NS::Certificate> signingCertificates(LIBRALF_NS::Error *_Nullable error) override;

    std::unique_ptr<LIBRALF_NS::IPackageReaderImpl> createReader(LIBRALF_NS::Error *_Nullable error) override;

    std::unique_ptr<LIBRALF_NS::IPackageAuxMetaDataImpl> auxMetaDataFile(std::string_view mediaType, size_t index,
                                                                         LIBRALF_NS::Error *_Nullable error) override;

    LIBRALF_NS::Result<size_t> auxMetaDataFileCount(std::string_view mediaType) override;

    LIBRALF_NS::Result<std::set<std::string>> auxMetaDataKeys() override;

    size_t maxExtractionBytes() const override;

    void setMaxExtractionBytes(size_t maxTotalSize) override;

    size_t maxExtractionEntries() const override;

    void setMaxExtractionEntries(size_t maxFileCount) override;

private:
    W3CPackage(int packageFd, std::shared_ptr<IZipReaderFactory> &&zipReaderFactory, LIBRALF_NS::Error *_Nullable error);

    static std::optional<std::vector<uint8_t>>
    readArchiveFileEntry(const std::unique_ptr<entos::ralf::archive::ILibarchiveReader> &archive, size_t maxSize,
                         LIBRALF_NS::Error *_Nullable error);

    static bool verifyArchiveFileEntry(const std::unique_ptr<entos::ralf::archive::ILibarchiveReader> &archive,
                                       size_t expectedSize, const std::vector<uint8_t> &digest,
                                       LIBRALF_NS::Error *_Nullable error);

    bool verifyFileContents(const std::filesystem::path &filePath, const std::vector<uint8_t> &fileContents,
                            LIBRALF_NS::Error *_Nullable error);

    bool readAndVerifySignature(const LIBRALF_NS::VerificationBundle &bundle, LIBRALF_NS::Package::OpenFlags flags,
                                LIBRALF_NS::Error *_Nullable error);

    std::shared_ptr<W3CPackageMetaDataImpl> readMetaData(LIBRALF_NS::Error *_Nullable error);

private:
    friend class W3CPackageReader;

    const std::filesystem::path m_signatureFileName;

    const std::shared_ptr<IZipReaderFactory> m_zipReaderFactory;

    int m_packageFd = -1;
    size_t m_packageSize = 0;
    mutable std::atomic<ssize_t> m_unpackedSize = -1;
    mutable std::mutex m_cachedDataLock;
    mutable std::shared_ptr<W3CPackageMetaDataImpl> m_metaData;
    std::list<LIBRALF_NS::Certificate> m_signingCerts;

    using FileDigestMap = std::map<std::filesystem::path, std::vector<uint8_t>>;
    std::shared_ptr<const FileDigestMap> m_digests;

    size_t m_maxExtractionBytes = SIZE_MAX;
    size_t m_maxExtractionEntries = SIZE_MAX;
};
