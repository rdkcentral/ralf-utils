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

#include "FileUtils.h"
#include "ShaUtils.h"

#include <fstream>

std::vector<uint8_t> fileContents(const std::filesystem::path &path, size_t offset, ssize_t size)
{
    std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file)
        return {};

    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0)
        return {};
    if (offset > static_cast<size_t>(fileSize))
        offset = fileSize;
    if (size < 0)
        size = static_cast<ssize_t>(fileSize - offset);

    file.seekg(static_cast<off_t>(offset), std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char *>(buffer.data()), size))
        return buffer;

    return {};
}

std::string fileStrContents(const std::filesystem::path &path, size_t offset, ssize_t size)
{
    const auto contents = fileContents(path, offset, size);
    return { reinterpret_cast<const char *>(contents.data()), contents.size() };
}

std::array<uint8_t, 32> fileSha256(const std::filesystem::path &path, size_t offset, ssize_t size)
{
    const auto contents = fileContents(path, offset, size);
    return sha256Sum(contents.data(), contents.size());
}
