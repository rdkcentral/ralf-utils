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

#include "Result.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class IOCIFileReader
{
public:
    virtual ~IOCIFileReader() = default;

    virtual ssize_t read(void *buf, size_t size) = 0;

    virtual int64_t seek(int64_t offset, int whence) = 0;

    virtual int64_t size() const = 0;
};

class IOCIBackingStore
{
public:
    virtual ~IOCIBackingStore() = default;

    virtual int64_t size() const = 0;

    virtual bool supportsMountableFiles() const = 0;

    virtual LIBRALF_NS::Result<std::vector<uint8_t>> readFile(const std::filesystem::path &path, size_t maxSize) const = 0;

    virtual LIBRALF_NS::Result<std::unique_ptr<IOCIFileReader>> getFile(const std::filesystem::path &path) const = 0;

    struct MappableFile
    {
        int fd = -1;
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    virtual LIBRALF_NS::Result<MappableFile> getMountableFile(const std::filesystem::path &path) const = 0;
};
