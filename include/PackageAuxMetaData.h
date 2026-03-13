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

#include "LibRalf.h"
#include "Result.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace LIBRALF_NS
{
    class IPackageAuxMetaDataImpl;

    // -------------------------------------------------------------------------
    /*!
        \class PackageAuxMetaData
        \brief Object representing auxiliary package meta-data.

        Auxiliary meta-data files are optional files that can be included in the
        package, they are identified by their mediaType. They are different from
        the main package meta-data (PackageMetaData), in that they may have
        content in any format and are not required to follow the same structure
        as the main package meta-data.

        A typically use case for auxiliary meta-data is to include additional
        data blobs that are used by middleware or other components need to
        launch and run the package.  The app, runtime or service itself does
        not get access to these files directly.

    */
    class LIBRALF_EXPORT PackageAuxMetaData
    {
    public:
        PackageAuxMetaData(const PackageAuxMetaData &) = delete;
        PackageAuxMetaData(PackageAuxMetaData &&other) noexcept;

        PackageAuxMetaData &operator=(const PackageAuxMetaData &) = delete;
        PackageAuxMetaData &operator=(PackageAuxMetaData &&other) noexcept;

        // -------------------------------------------------------------------------
        /*!
            Destructs the PackageAuxMetaData object.

         */
        ~PackageAuxMetaData();

        // -------------------------------------------------------------------------
        /*!
            Returns the index of the auxiliary meta-data file within the package.
            This is the index of the file with the same media type, starting from 0.

         */
        size_t index() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the media type of the auxiliary meta-data file.

         */
        std::string mediaType() const;

        // -------------------------------------------------------------------------
        /*!
            Returns a map of any annotations associated with the auxiliary meta-data
            file.

        */
        std::map<std::string, std::string> annotations() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the size of the auxiliary meta-data file in bytes.

         */
        size_t size() const;

        // -------------------------------------------------------------------------
        /*!
            Reads the auxiliary meta-data file into the given buffer, up to \a size
            bytes.  The buffer must be large enough to hold the data, otherwise
            this will return an error.

            The read advances the internal offset of the file, so subsequent reads
            will continue from where the last read left off.

            Returns the number of bytes read or an error if failed to read.

         */
        Result<size_t> read(void *buf, size_t size);

        // -------------------------------------------------------------------------
        /*!
            Seeks to the given \a position in the auxiliary meta-data file.

         */
        Result<> seek(off_t position);

        // -------------------------------------------------------------------------
        /*!
            Reads all the contents of the auxiliary meta-data file into a vector of
            bytes.  This does not affect the current read offset, so subsequent
            reads will still start from previous read position.

         */
        Result<std::vector<uint8_t>> readAll();

    private:
        friend class Package;
        explicit PackageAuxMetaData(std::unique_ptr<IPackageAuxMetaDataImpl> &&impl);

    private:
        std::unique_ptr<IPackageAuxMetaDataImpl> m_impl;
    };

} // namespace LIBRALF_NS
