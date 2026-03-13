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

#include <gmock/gmock.h>

#include <filesystem>
#include <memory>
#include <string_view>

#include <sys/stat.h>
#include <unistd.h>

class MockOpenFile;

// -------------------------------------------------------------------------
/*!
    \class MockFile
    \brief Test helper class to replace _some_ of the posix file functions.

    \warning This class uses global state to track the mocked files, so tests
    using this class should not be run in parallel.  This mock is NOT thread
    safe.

    It's purpose is to allow tests to fake a file on disk, so that they can
    mock the read / write / fstat / ioctl functions etc.

    This is only very basic and doesn't attempt to cover all the posix file
    functions, just the ones we need to mock out in our tests.

    To use this, first call the create() method to create a MockFile instance
    for a given file path.  Then set up expectations on the returned object
    for the various file operations you expect to be called on that file.

 */
class MockFile
{
public:
    static std::shared_ptr<MockFile> registerPath(const std::filesystem::path &path);
    static void clearAllMocks();

public:
    MOCK_METHOD(std::shared_ptr<MockOpenFile>, open, (const char *file, int flags, mode_t mode));

    MOCK_METHOD(int, stat, (const char *file, struct stat *buf));
};

// -------------------------------------------------------------------------
/*!
    \class MockOpenFile
    \brief Mock implementation of an opened file.

    This class represents an opened file descriptor, and allows mocking of
    the various file operations that can be performed on it.

 */
class MockOpenFile
{
public:
    int fd() const { return m_mockedFd; }

    MOCK_METHOD(int, close, ());

    MOCK_METHOD(int, ioctl, (unsigned long request, void *arg));
    MOCK_METHOD(int, fstat, (struct stat * buf));

    MOCK_METHOD(ssize_t, read, (void *buf, size_t nbytes));
    MOCK_METHOD(ssize_t, pread, (void *buf, size_t nbytes, off_t offset));

    MOCK_METHOD(ssize_t, write, (const void *buf, size_t n));
    MOCK_METHOD(ssize_t, pwrite, (const void *buf, size_t n, off_t offset));

private:
    friend int doOpenAt(int dirfd, const char *path, int flags, mode_t mode);
    int m_mockedFd = -1;
};
