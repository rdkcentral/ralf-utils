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

#include "IOCIBackingStore.h"
#include "core/IPackageReaderImpl.h"
#include "erofs/IErofsReader.h"

#include <cstdint>
#include <memory>
#include <vector>

// -------------------------------------------------------------------------
/*!
    \class OCIErofsImageLayerReader
    \brief Implement of a PackageReader that reads from an OCI EROFS image layer.

    For example if the OCI package has a layer with type erofs+dm-verity,
    then this is the reader that will be used to read the contents of the layer.

    The reader is required to ensure that every block of data read from the
    EROFS image is checked against the dm-verity hash tree, so that we can
    ensure that the data is valid and has not been tampered with.

    The constructor is expected to be supplied with a signature checked OCI
    descriptor, which contains annotations for the dm-verity root hash as
    well as offsets and the salt value (although the salt is typically
    include in the dm-verity superblock).

*/
class OCIErofsImageLayerReader final : public LIBRALF_NS::IPackageReaderImpl
{
public:
    OCIErofsImageLayerReader(const IOCIBackingStore::MappableFile &imageFile, uint64_t hashesOffset,
                             const std::vector<uint8_t> &rootHash);
    ~OCIErofsImageLayerReader() final;

    std::unique_ptr<LIBRALF_NS::IPackageEntryImpl> next() override;

    bool hasError() const override;

    LIBRALF_NS::Error error() const override;

private:
    void setError(std::error_code code, const char *format, ...) __attribute__((format(printf, 3, 4)));

private:
    std::shared_ptr<entos::ralf::erofs::IErofsReader> m_erofsReader;
    LIBRALF_NS::Error m_error;
};
