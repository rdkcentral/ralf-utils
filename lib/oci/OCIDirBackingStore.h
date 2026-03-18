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

// -------------------------------------------------------------------------
/*!
    \class OCIDirBackingStore
    \brief Implement of IOCIBackingStore that reads files from a directory.

    This is just a simple wrapper around file reads from a directory.

*/
class OCIDirBackingStore final : public IOCIBackingStore
{
public:
    explicit OCIDirBackingStore(std::filesystem::path directoryPath);
    ~OCIDirBackingStore() final = default;

    int64_t size() const override;

    bool supportsMountableFiles() const override;

    LIBRALF_NS::Result<std::vector<uint8_t>> readFile(const std::filesystem::path &path, size_t maxSize) const override;

    LIBRALF_NS::Result<std::unique_ptr<IOCIFileReader>> getFile(const std::filesystem::path &path) const override;

    LIBRALF_NS::Result<std::unique_ptr<IOCIMappableFile>> getMappableFile(const std::filesystem::path &path) const override;

private:
    const std::filesystem::path m_baseDir;
};
