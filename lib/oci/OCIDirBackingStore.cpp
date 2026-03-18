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

#include "OCIDirBackingStore.h"
#include "OCIMappableFile.h"

#include "core/Compatibility.h"
#include "core/LogMacros.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

// -------------------------------------------------------------------------
/*!
    \class SimpleFileReader
    \brief Instance of IOCIFileReader that wraps a file for reading.

    This object is just a basic wrapper around a file descriptor.

 */
class SimpleFileReader final : public IOCIFileReader
{
public:
    SimpleFileReader(int fd, int64_t size)
        : m_fd(fd)
        , m_size(size)
    {
    }

    ~SimpleFileReader() final
    {
        if (close(m_fd) != 0)
            logSysError(errno, "Failed to close file descriptor");
    }

    ssize_t read(void *buf, size_t size) override { return TEMP_FAILURE_RETRY(::read(m_fd, buf, size)); }

    int64_t seek(int64_t offset, int whence) override { return lseek(m_fd, offset, whence); }

    int64_t size() const override { return m_size; }

private:
    int m_fd;
    int64_t m_size;
};

OCIDirBackingStore::OCIDirBackingStore(std::filesystem::path directoryPath)
    : m_baseDir(std::move(directoryPath))
{
}

int64_t OCIDirBackingStore::size() const
{
    // FIXME: what should we return for an extracted directory?
    return 0;
}

bool OCIDirBackingStore::supportsMountableFiles() const
{
    return true;
}

Result<std::vector<uint8_t>> OCIDirBackingStore::readFile(const std::filesystem::path &path, size_t maxSize) const
{
    std::filesystem::path fullPath = m_baseDir / path;
    if (!std::filesystem::exists(fullPath))
        return Error(ErrorCode::PackageInvalidEntry, "File not found");

    if (!std::filesystem::is_regular_file(fullPath))
        return Error(ErrorCode::PackageContentsInvalid, "Path is a not a regular file");

    int fd = open(fullPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return Error(std::error_code(errno, std::system_category()), "Failed to open file");

    struct stat st = {};
    if ((fstat(fd, &st) < 0) || (st.st_size < 0))
    {
        close(fd);
        return Error(std::error_code(errno, std::system_category()), "Failed to stat file");
    }

    const auto fileSize = static_cast<size_t>(st.st_size);
    if (fileSize > maxSize)
    {
        close(fd);
        return Error(ErrorCode::PackageFileTooLarge, "File is too large");
    }

    std::vector<uint8_t> buffer(fileSize);

    size_t rd = 0;
    while (rd < fileSize)
    {
        ssize_t bytesRead = TEMP_FAILURE_RETRY(read(fd, buffer.data() + rd, fileSize - rd));
        if (bytesRead <= 0)
        {
            close(fd);
            return Error(std::error_code(errno, std::system_category()), "Failed to read file");
        }

        rd += static_cast<size_t>(bytesRead);
    }

    close(fd);

    return buffer;
}

Result<std::unique_ptr<IOCIFileReader>> OCIDirBackingStore::getFile(const std::filesystem::path &path) const
{
    std::filesystem::path fullPath = m_baseDir / path;
    if (!std::filesystem::exists(fullPath))
        return Error(ErrorCode::PackageInvalidEntry, "File not found");

    if (!std::filesystem::is_regular_file(fullPath))
        return Error(ErrorCode::PackageContentsInvalid, "Path is a not a regular file");

    int fd = open(fullPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return Error(std::error_code(errno, std::system_category()), "Failed to open file");

    struct stat st = {};
    if (fstat(fd, &st) < 0)
    {
        close(fd);
        return Error(std::error_code(errno, std::system_category()), "Failed to stat file");
    }

    return std::make_unique<SimpleFileReader>(fd, st.st_size);
}

Result<std::unique_ptr<IOCIMappableFile>> OCIDirBackingStore::getMappableFile(const std::filesystem::path &path) const
{
    std::filesystem::path fullPath = m_baseDir / path;
    if (!std::filesystem::exists(fullPath))
        return Error(ErrorCode::PackageInvalidEntry, "File not found");

    if (!std::filesystem::is_regular_file(fullPath))
        return Error(ErrorCode::PackageContentsInvalid, "Path is a not a regular file");

    int fd = open(fullPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return Error(std::error_code(errno, std::system_category()), "Failed to open file");

    struct stat st = {};
    if (fstat(fd, &st) < 0)
    {
        close(fd);
        return Error(std::error_code(errno, std::system_category()), "Failed to stat file");
    }

    return std::make_unique<OCIMappableFile>(fd, 0, static_cast<uint64_t>(st.st_size));
}
