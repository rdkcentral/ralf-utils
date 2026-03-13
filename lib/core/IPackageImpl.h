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

#include "Compatibility.h"
#include "Error.h"
#include "IPackageAuxMetaDataImpl.h"
#include "IPackageMetaDataImpl.h"
#include "IPackageMountImpl.h"
#include "IPackageReaderImpl.h"
#include "LibRalf.h"
#include "Package.h"
#include "PackageMount.h"

#include <cinttypes>
#include <filesystem>
#include <memory>

namespace LIBRALF_NS
{

    class IPackageImpl
    {
    public:
        virtual ~IPackageImpl() = default;

        virtual bool isMountable() const = 0;

        virtual Package::Format format() const = 0;

        virtual ssize_t size() const = 0;

        virtual ssize_t unpackedSize() const = 0;

        virtual bool verify(Error *_Nullable error) = 0;

        virtual Result<std::unique_ptr<IPackageMountImpl>> mount(const std::filesystem::path &mountPoint,
                                                                 MountFlags flags) = 0;

        virtual std::shared_ptr<IPackageMetaDataImpl> metaData(Error *_Nullable error) = 0;

        virtual std::list<Certificate> signingCertificates(Error *_Nullable error) = 0;

        virtual std::unique_ptr<IPackageReaderImpl> createReader(Error *_Nullable error) = 0;

        virtual std::unique_ptr<IPackageAuxMetaDataImpl> auxMetaDataFile(std::string_view mediaType, size_t index,
                                                                         Error *_Nullable error) = 0;

        virtual Result<size_t> auxMetaDataFileCount(std::string_view mediaType) = 0;

        virtual Result<std::set<std::string>> auxMetaDataKeys() = 0;

        virtual size_t maxExtractionBytes() const = 0;
        virtual void setMaxExtractionBytes(size_t maxTotalSize) = 0;

        virtual size_t maxExtractionEntries() const = 0;
        virtual void setMaxExtractionEntries(size_t maxFileCount) = 0;
    };

} // namespace LIBRALF_NS
