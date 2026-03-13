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

#include "OCIErofsImageEntry.h"
#include "core/LogMacros.h"
#include "core/Utils.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <climits>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::erofs;

OCIErofsImageEntry::OCIErofsImageEntry(std::unique_ptr<IErofsEntry> &&entry)
    : m_entryRef(std::move(entry))
    , m_entry(m_entryRef.get())
    , m_offset(0)
{
}

const std::filesystem::path &OCIErofsImageEntry::path() const
{
    return m_entry->path();
}

size_t OCIErofsImageEntry::size() const
{
    return m_entry->size();
}

std::filesystem::perms OCIErofsImageEntry::permissions() const
{
    return m_entry->permissions();
}

time_t OCIErofsImageEntry::modificationTime() const
{
    return m_entry->modificationTime();
}

uid_t OCIErofsImageEntry::ownerId() const
{
    return m_entry->ownerId();
}

gid_t OCIErofsImageEntry::groupId() const
{
    return m_entry->groupId();
}

std::filesystem::file_type OCIErofsImageEntry::type() const
{
    return m_entry->type();
}

ssize_t OCIErofsImageEntry::read(void *_Nullable buf, size_t size, Error *_Nullable error)
{
    ssize_t bytesRead = m_entry->read(buf, size, m_offset, error);
    if (bytesRead > 0)
        m_offset += bytesRead;

    return bytesRead;
}

Result<size_t> OCIErofsImageEntry::writeTo(int directoryFd, size_t maxSize, Package::ExtractOptions options)
{
    // The following checks if the entry can be extracted to the given directory based on the supplied extraction
    // options, it will return either an error or an ExtractOperation indicating what to do with the entry. This will
    // also remove any existing file or directory at the target path if the options allow it.
    auto result = checkExtraction(directoryFd, m_entry->path(), m_entry->type(), m_entry->permissions(), options);
    if (!result)
        return result.error();

    if (result.value() == ExtractOperation::Skip)
        return Ok(static_cast<size_t>(0));

    const auto type = m_entry->type();
    const auto mode = modeFromFsPerms(m_entry->permissions());
    const auto &path = m_entry->path();

    size_t amountWritten;

    if (type == std::filesystem::file_type::regular)
    {
        // Open the file for writing
        int fd = openat(directoryFd, path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, mode);
        if (fd < 0)
        {
            return Error::format(std::error_code(errno, std::system_category()), "Failed to create file '%s'",
                                 path.c_str());
        }

        // Read and write the file contents
        Error error;
        uint8_t buffer[4096];
        size_t offset = 0;
        const size_t fileSize = m_entry->size();

        while (offset < fileSize)
        {
            ssize_t bytesRead = m_entry->read(buffer, sizeof(buffer), offset, &error);
            if (error || bytesRead <= 0)
            {
                close(fd);
                return Error::format(error.code(), "Failed to read file '%s' - %s", path.c_str(), error.what());
            }

            if ((offset + bytesRead) > maxSize)
            {
                close(fd);
                return Error::format(ErrorCode::PackageContentsInvalid, "File '%s' exceeds maximum allowed size",
                                     path.c_str());
            }

            ssize_t bytesWritten = TEMP_FAILURE_RETRY(write(fd, buffer, bytesRead));
            if (bytesWritten != bytesRead)
            {
                auto err = std::error_code(errno, std::system_category());

                close(fd);
                return Error::format(err, "Failed to write to file '%s'", path.c_str());
            }

            offset += bytesRead;
        }

        // Set the file permissions
        if (fchmod(fd, mode) != 0)
        {
            auto err = std::error_code(errno, std::system_category());

            close(fd);
            return Error::format(err, "Failed to set file mode for file '%s'", m_entry->path().c_str());
        }

        amountWritten = offset;

        close(fd);
    }
    else if (type == std::filesystem::file_type::directory)
    {
        // Create the directory
        if (mkdirat(directoryFd, path.c_str(), 0755) != 0)
        {
            return Error::format(std::error_code(errno, std::system_category()), "Failed to create directory '%s'",
                                 path.c_str());
        }

        amountWritten = 0;
    }
    else if (type == std::filesystem::file_type::symlink)
    {
        Error error;

        // Read the symlink target from the entry
        char target[PATH_MAX];
        ssize_t targetSize = m_entry->read(target, sizeof(target) - 1, 0, &error);
        if (error || targetSize <= 0)
        {
            return Error::format(ErrorCode::ErofsError, "Failed to read symlink target for '%s' - %s", path.c_str(),
                                 error.what());
        }

        if (static_cast<size_t>(targetSize) > maxSize)
        {
            return Error::format(ErrorCode::ErofsError, "Symlink target for '%s' is too long", path.c_str());
        }

        target[targetSize] = '\0';

        // Create the symlink
        if (symlinkat(target, directoryFd, path.c_str()) != 0)
        {
            return Error::format(std::error_code(errno, std::system_category()), "Failed to create symlink '%s'",
                                 path.c_str());
        }

        amountWritten = targetSize;
    }
    else
    {
        return Error::format(ErrorCode::NotSupported, "Unsupported file type %d for entry '%s'", int(type), path.c_str());
    }

    // Set the file modification time
    struct timespec times[2];
    times[0].tv_sec = 0;
    times[0].tv_nsec = UTIME_OMIT;
    times[1].tv_sec = m_entry->modificationTime();
    times[1].tv_nsec = 0;

    if (utimensat(directoryFd, path.c_str(), times, AT_SYMLINK_NOFOLLOW) != 0)
        logSysWarning(errno, "Failed to set modification time for '%s'", m_entry->path().c_str());

    return Ok(amountWritten);
}
