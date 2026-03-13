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

#include "IPackageAuxMetaDataImpl.h"
#include "LibRalf.h"

#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

namespace LIBRALF_NS
{

    // -------------------------------------------------------------------------
    /*!
        \class PackageAuxMetaDataImpl
        \brief Implements a generic auxiliary meta-data wrapper around a vector
        of bytes.

        Individual package objects may provide their own implementations of
        auxiliary meta-data that reads the data in a more efficient way, this
        version is a generic implementation.

    */
    class PackageAuxMetaDataImpl final : public IPackageAuxMetaDataImpl
    {
    public:
        PackageAuxMetaDataImpl(std::string_view mediaType, size_t index, std::vector<uint8_t> data,
                               std::map<std::string, std::string> annotations = {});
        ~PackageAuxMetaDataImpl() final = default;

        size_t index() const final;

        std::string mediaType() const final;

        std::map<std::string, std::string> annotations() const final;

        size_t size() const final;

        Result<size_t> read(void *buf, size_t size) final;

        Result<> seek(off_t position) final;

        Result<std::vector<uint8_t>> readAll() final;

    private:
        const std::string m_mediaType;
        const size_t m_index;
        const std::vector<uint8_t> m_data;
        const std::map<std::string, std::string> m_annotations;

        size_t m_currentOffset = 0;
    };

} // namespace LIBRALF_NS
