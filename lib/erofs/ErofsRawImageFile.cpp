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

#include "ErofsRawImageFile.h"

#include "Error.h"
#include "core/Compatibility.h"
#include "core/LogMacros.h"

#include <fcntl.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::erofs;

Result<std::unique_ptr<ErofsRawImageFile>>
ErofsRawImageFile::open(const std::filesystem::path &filePath, std::optional<size_t> offset, std::optional<size_t> size)
{
    int fd = ::open(filePath.c_str(), O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        return Error::format(std::error_code(errno, std::system_category()), "Failed to open EROFS image file '%s'",
                             filePath.c_str());
    }

    auto file = ErofsRawImageFile::open(fd, offset, size);
    close(fd);

    return file;
}

Result<std::unique_ptr<ErofsRawImageFile>> ErofsRawImageFile::open(int fd, std::optional<size_t> offset,
                                                                   std::optional<size_t> size)
{
    // Get the size of the file
    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize < 0)
    {
        auto err = std::error_code(errno, std::system_category());
        close(fd);

        return Error(err, "Failed to seek to end of file");
    }

    // If an offset and / or size was supplied then check they're within the file bounds
    size_t actualOffset = offset.value_or(0);
    if (actualOffset > static_cast<size_t>(fileSize))
    {
        return Error::format(ErrorCode::ErofsError, "File offset is outside of the file size (offset %zu, size %zu)",
                             actualOffset, static_cast<size_t>(fileSize));
    }

    size_t actualSize = size.value_or(fileSize - actualOffset);
    if (actualSize > (fileSize - actualOffset))
    {
        return Error::format(ErrorCode::ErofsError,
                             "Supplied file range is outside of the file size (offset %zu, size %zu, actual size %zu)",
                             actualOffset, actualSize, static_cast<size_t>(fileSize));
    }

    int dupedFd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    if (dupedFd < 0)
    {
        return Error(std::error_code(errno, std::system_category()), "Failed to dup file descriptor");
    }

    // Return the wrapped fd (ErofsRawImageFile takes ownership)
    return std::unique_ptr<ErofsRawImageFile>(new ErofsRawImageFile(dupedFd, actualOffset, actualSize));
}

ErofsRawImageFile::ErofsRawImageFile(int fd, size_t offset, size_t size)
    : m_fd(fd)
    , m_offset(offset)
    , m_size(size)
{
}

ErofsRawImageFile::~ErofsRawImageFile()
{
    if (close(m_fd) != 0)
        logSysError(errno, "failed to close fd");
}

size_t ErofsRawImageFile::size() const
{
    return m_size;
}

bool ErofsRawImageFile::read(void *buffer, size_t size, size_t offset)
{
    if ((offset >= m_size) || (size > (m_size - offset)))
        return false;
    else
        return (TEMP_FAILURE_RETRY(pread(m_fd, buffer, size, static_cast<off_t>(m_offset + offset))) ==
                static_cast<ssize_t>(size));
}

bool ErofsRawImageFile::advise(Advice advice, size_t size, size_t offset)
{
#if !defined(EMSCRIPTEN) && defined(__linux__)
    int advice_;
    switch (advice)
    {
        case Advice::Normal:
            advice_ = POSIX_FADV_NORMAL;
            break;
        case Advice::Sequential:
            advice_ = POSIX_FADV_SEQUENTIAL;
            break;
        case Advice::Random:
            advice_ = POSIX_FADV_RANDOM;
            break;
        case Advice::WillNeed:
            advice_ = POSIX_FADV_WILLNEED;
            break;
        case Advice::DontNeed:
            advice_ = POSIX_FADV_DONTNEED;
            break;

        default:
            // not possible - only needed to fix bogus gcc warning
            return false;
    }

    int ret = posix_fadvise(m_fd, static_cast<off_t>(offset), static_cast<off_t>(size), advice_);
    if (ret != 0)
    {
        logSysWarning(ret, "posix_fadvise failed on range %zu : %zu with advice %d", offset, size, advice_);

        errno = ret;
        return false;
    }
#endif

    return true;
}
