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
#include "Package.h"
#include "PackageEntry.h"

namespace LIBRALF_NS
{

    class IPackageEntryImpl
    {
    public:
        virtual ~IPackageEntryImpl() = default;

        virtual const std::filesystem::path &path() const = 0;

        virtual size_t size() const = 0;

        virtual std::filesystem::perms permissions() const = 0;

        virtual time_t modificationTime() const = 0;

        virtual uid_t ownerId() const = 0;
        virtual gid_t groupId() const = 0;

        virtual std::filesystem::file_type type() const = 0;

        virtual ssize_t read(void *_Nullable buf, size_t size, Error *_Nullable error) = 0;

        virtual Result<size_t> writeTo(int directoryFd, size_t maxSize, Package::ExtractOptions options) = 0;
    };

} // namespace LIBRALF_NS