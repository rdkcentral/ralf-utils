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

#include "MockFile.h"

#include <cstdarg>
#include <map>
#include <string>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#if defined(__linux__)
#    include <sys/syscall.h>
#endif

static std::map<std::filesystem::path, std::weak_ptr<MockFile>> g_mockedFiles;
static std::map<int, std::weak_ptr<MockOpenFile>> g_mockedOpenFiles;

// -------------------------------------------------------------------------
/*!
    Registers a new MockFile instance for the given file path. If an
    instance already exists for that path, it is replaced.  Instances already
    attached to mock file descriptors are unaffected.

 */
std::shared_ptr<MockFile> MockFile::registerPath(const std::filesystem::path &path)
{
    auto mockFile = std::make_shared<MockFile>();
    g_mockedFiles[path] = mockFile;
    return mockFile;
}

// -------------------------------------------------------------------------
/*!
    Removes all registered mock files and open file descriptors.

*/
void MockFile::clearAllMocks()
{
    g_mockedFiles.clear();
    g_mockedOpenFiles.clear();
}

static std::shared_ptr<MockOpenFile> getMockedOpenFile(int fd)
{
    auto it = g_mockedOpenFiles.find(fd);
    if (it != g_mockedOpenFiles.end())
    {
        auto openFile = it->second.lock();
        if (!openFile)
        {
            g_mockedOpenFiles.erase(it);
        }
        else
        {
            return openFile;
        }
    }

    return nullptr;
}

#if defined(__linux__)

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Creates a "mock" file descriptor by opening /dev/null.  This is used so
    we don't have a clash with any real file descriptors opened by the test
    code.

*/
static int createMockFd()
{
    int fd = (int)syscall(SYS_openat, int(AT_FDCWD), "/dev/null", int(O_CLOEXEC | O_RDONLY), 0);
    if (fd < 0)
        throw std::runtime_error("Failed to create mock file descriptor");

    return fd;
}

// -------------------------------------------------------------------------
/*!
    The following are the intercepted posix file functions that will check
    if the file being operated on is a mocked file, and if so will route the
    call to the appropriate MockFile / MockOpenFile instance.  If the file
    is not mocked, then the real posix function is called instead.

 */

// NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)

int doOpenAt(int dirfd, const char *path, int flags, mode_t mode)
{
    auto it = g_mockedFiles.find(path);
    if (it != g_mockedFiles.end())
    {
        auto mockFile = it->second.lock();
        if (!mockFile)
        {
            g_mockedFiles.erase(it);
        }
        else
        {
            auto openFile = mockFile->open(path, flags, mode);
            if (!openFile)
            {
                errno = ENOENT;
                return -1;
            }

            int mockFd = createMockFd();
            openFile->m_mockedFd = mockFd;
            g_mockedOpenFiles[mockFd] = openFile;

            return mockFd;
        }
    }

    return (int)syscall(SYS_openat, dirfd, path, flags, mode);
}

extern "C" int stat(const char *file, struct stat *buf)
{
    auto it = g_mockedFiles.find(file);
    if (it != g_mockedFiles.end())
    {
        auto mockFile = it->second.lock();
        if (mockFile)
        {
            return mockFile->stat(file, buf);
        }
        else
        {
            g_mockedFiles.erase(it);
        }
    }

    return (int)syscall(SYS_newfstatat, AT_FDCWD, file, buf, 0);
}

extern "C" int open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    return doOpenAt(AT_FDCWD, path, flags, mode);
}

extern "C" int openat(int dirfd, const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    return doOpenAt(dirfd, path, flags, mode);
}

extern "C" int close(int fd)
{
    auto it = g_mockedOpenFiles.find(fd);
    if (it != g_mockedOpenFiles.end())
    {
        auto mockOpenFile = it->second.lock();
        if (mockOpenFile)
            mockOpenFile->close();

        g_mockedOpenFiles.erase(it);
    }

    return (int)syscall(SYS_close, fd);
}

extern "C" int ioctl(int fd, unsigned long int request, ...)
{
    void *arg;
    va_list ap;
    va_start(ap, request);
    arg = va_arg(ap, void *);
    va_end(ap);

    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->ioctl(request, arg);
    }
    else
    {
        return (int)syscall(SYS_ioctl, fd, request, arg);
    }
}

extern "C" int fstat(int fd, struct stat *buf)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->fstat(buf);
    }
    else
    {
        return (int)syscall(SYS_fstat, fd, buf);
    }
}

extern "C" ssize_t read(int fd, void *buf, size_t n)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->read(buf, n);
    }
    else
    {
        return (int)syscall(SYS_read, fd, buf, n);
    }
}

extern "C" ssize_t write(int fd, const void *buf, size_t n)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->write(buf, n);
    }
    else
    {
        return (int)syscall(SYS_write, fd, buf, n);
    }
}

extern "C" ssize_t pread(int fd, void *buf, size_t n, __off_t offset)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->pread(buf, n, offset);
    }
    else
    {
        return (int)syscall(SYS_pread64, fd, buf, n, offset);
    }
}

extern "C" ssize_t pwrite(int fd, const void *buf, size_t n, __off_t offset)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->pwrite(buf, n, offset);
    }
    else
    {
        return (int)syscall(SYS_pwrite64, fd, buf, n, offset);
    }
}

// NOLINTEND(readability-inconsistent-declaration-parameter-name)

#elif defined(__APPLE__)

// -------------------------------------------------------------------------
/*!
    On OSX it was a bit tricky to interpose a function in an app itself.
    After a bit of experimentation found that the most reliable way was
    to get a library and use the DYLD_INTERPOSE macro to replace the time
    functions.

    \see https://medium.com/geekculture/code-injection-with-dyld-interposing-3008441c62dd
 */
#    define DYLD_INTERPOSE(_replacement, _replacee)                                                                    \
        __attribute__((used)) static struct                                                                            \
        {                                                                                                              \
            const void *replacement;                                                                                   \
            const void *replacee;                                                                                      \
        } _interpose_##_replacee                                                                                       \
            __attribute__((section("__DATA,__interpose"))) = { (const void *)(unsigned long)&_replacement,             \
                                                               (const void *)(unsigned long)&_replacee };

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Creates a "mock" file descriptor by opening /dev/null.  This is used so
    we don't have a clash with any real file descriptors opened by the test
    code.

 */
static int createMockFd()
{
    int fd = open("/dev/null", O_CLOEXEC | O_RDONLY);
    if (fd < 0)
        throw std::runtime_error("Failed to create mock file descriptor");

    return fd;
}

int doOpenAt(int dirfd, const char *path, int flags, mode_t mode)
{
    auto it = g_mockedFiles.find(path);
    if (it != g_mockedFiles.end())
    {
        auto mockFile = it->second.lock();
        if (!mockFile)
        {
            g_mockedFiles.erase(it);
        }
        else
        {
            auto openFile = mockFile->open(path, flags, mode);
            if (!openFile)
            {
                errno = ENOENT;
                return -1;
            }

            int mockFd = createMockFd();
            openFile->m_mockedFd = mockFd;
            g_mockedOpenFiles[mockFd] = openFile;

            return mockFd;
        }
    }

    return openat(dirfd, path, flags, mode);
}

extern "C" int FakeStat(const char *file, struct stat *buf)
{
    auto it = g_mockedFiles.find(file);
    if (it != g_mockedFiles.end())
    {
        auto mockFile = it->second.lock();
        if (mockFile)
        {
            return mockFile->stat(file, buf);
        }
        else
        {
            g_mockedFiles.erase(it);
        }
    }

    return stat(file, buf);
}
DYLD_INTERPOSE(FakeStat, stat);

extern "C" int FakeOpen(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    return doOpenAt(AT_FDCWD, path, flags, mode);
}
DYLD_INTERPOSE(FakeOpen, open);

extern "C" int FakeOpenAt(int dirfd, const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    return doOpenAt(dirfd, path, flags, mode);
}
DYLD_INTERPOSE(FakeOpenAt, openat);

extern "C" int FakeClose(int fd)
{
    auto it = g_mockedOpenFiles.find(fd);
    if (it != g_mockedOpenFiles.end())
    {
        auto mockOpenFile = it->second.lock();
        if (mockOpenFile)
            mockOpenFile->close();

        g_mockedOpenFiles.erase(it);
    }

    return close(fd);
}
DYLD_INTERPOSE(FakeClose, close);

extern "C" int FakeIoctl(int fd, unsigned long int request, ...)
{
    void *arg;
    va_list ap;
    va_start(ap, request);
    arg = va_arg(ap, void *);
    va_end(ap);

    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->ioctl(request, arg);
    }
    else
    {
        return ioctl(fd, request, arg);
    }
}
DYLD_INTERPOSE(FakeIoctl, ioctl)

extern "C" int FakeFStat(int fd, struct stat *buf)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->fstat(buf);
    }
    else
    {
        return fstat(fd, buf);
    }
}
DYLD_INTERPOSE(FakeFStat, fstat)

extern "C" ssize_t FakeRead(int fd, void *buf, size_t n)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->read(buf, n);
    }
    else
    {
        return read(fd, buf, n);
    }
}
DYLD_INTERPOSE(FakeRead, read)

extern "C" ssize_t FakeWrite(int fd, const void *buf, size_t n)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->write(buf, n);
    }
    else
    {
        return write(fd, buf, n);
    }
}
DYLD_INTERPOSE(FakeWrite, write)

extern "C" ssize_t FakePRead(int fd, void *buf, size_t n, off_t offset)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->pread(buf, n, offset);
    }
    else
    {
        return pread(fd, buf, n, offset);
    }
}
DYLD_INTERPOSE(FakePRead, pread)

extern "C" ssize_t FakePWrite(int fd, const void *buf, size_t n, off_t offset)
{
    auto mockOpenFile = getMockedOpenFile(fd);
    if (mockOpenFile)
    {
        return mockOpenFile->pwrite(buf, n, offset);
    }
    else
    {
        return pwrite(fd, buf, n, offset);
    }
}
DYLD_INTERPOSE(FakePWrite, pwrite)

#endif // defined(__APPLE__)
