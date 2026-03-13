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

#include "archive/ArchiveReader.h"
#include "archive/ILibarchiveReader.h"
#include "archive/LibarchiveFileReader.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

TEST(LibarchiveFileReaderTest, TestOpenArchive)
{
    int fd = open(TEST_DATA_DIR "/archives/random_files.tar", O_CLOEXEC | O_RDONLY);
    ASSERT_GE(fd, 0) << "Failed to open test archive file - " << strerror(errno);

    struct stat st = {};
    ASSERT_EQ(fstat(fd, &st), 0) << "Failed to stat test archive file - " << strerror(errno);
    ASSERT_GT(st.st_size, 0) << "Test archive file is empty";

    Error err;
    auto archiveReader = std::make_unique<LibarchiveFileReader>(fd, st.st_size, ArchiveFormats::All, true, &err);
    ASSERT_NE(archiveReader, nullptr);
    ASSERT_FALSE(archiveReader->isNull()) << err.what();

    archive_entry *entry = nullptr;
    EXPECT_EQ(archiveReader->nextHeader(&entry), 0);

    EXPECT_EQ(archiveReader->format(), ArchiveFormat::Tarball) << "Unexpected archive format";
}

TEST(LibarchiveFileReaderTest, TestOpenUnsupportedArchives)
{
    {
        int zipFd = open(TEST_DATA_DIR "/archives/random_files.zip", O_CLOEXEC | O_RDONLY);
        ASSERT_GE(zipFd, 0) << "Failed to open test archive file - " << strerror(errno);

        struct stat st = {};
        ASSERT_EQ(fstat(zipFd, &st), 0) << "Failed to stat test archive file - " << strerror(errno);
        ASSERT_GT(st.st_size, 0) << "Test archive file is empty";

        Error err;
        auto archiveReader = std::make_unique<LibarchiveFileReader>(zipFd, st.st_size,
                                                                    ArchiveFormats::Tarball | ArchiveFormats::TarballGzip |
                                                                        ArchiveFormats::TarballZstd,
                                                                    true, &err);
        EXPECT_TRUE(archiveReader->isNull());
        EXPECT_TRUE(err);
    }

    {
        int tarFd = open(TEST_DATA_DIR "/archives/random_files.tar", O_CLOEXEC | O_RDONLY);
        ASSERT_GE(tarFd, 0) << "Failed to open test archive file - " << strerror(errno);

        struct stat st = {};
        ASSERT_EQ(fstat(tarFd, &st), 0) << "Failed to stat test archive file - " << strerror(errno);
        ASSERT_GT(st.st_size, 0) << "Test archive file is empty";

        Error err;
        auto archiveReader = std::make_unique<LibarchiveFileReader>(tarFd, st.st_size, ArchiveFormats::Zip, true, &err);
        EXPECT_TRUE(archiveReader->isNull());
        EXPECT_TRUE(err);
    }
}

TEST(LibarchiveFileReaderTest, TestAbsolutePathsInArchive)
{
    std::vector<std::filesystem::path> testPaths = { TEST_DATA_DIR "/archives/absolute_paths.tar",
                                                     TEST_DATA_DIR "/archives/absolute_paths.zip" };

    for (const auto &testPath : testPaths)
    {
        int fd = open(testPath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(fd, 0) << "Failed to open test archive file " << testPath << " - " << strerror(errno);

        struct stat st = {};
        ASSERT_EQ(fstat(fd, &st), 0) << "Failed to stat test archive file " << testPath << " - " << strerror(errno);
        ASSERT_GT(st.st_size, 0) << "Test archive file is empty";

        Error err;
        auto reader = std::make_unique<LibarchiveFileReader>(fd, st.st_size, ArchiveFormats::All, true, &err);
        ASSERT_NE(reader, nullptr);
        ASSERT_FALSE(reader->isNull()) << err.what();

        ArchiveReader archiveReader(std::move(reader));
        ASSERT_FALSE(archiveReader.hasError()) << archiveReader.error().what();

        std::unique_ptr<IPackageEntryImpl> entry;
        while ((entry = archiveReader.next()) != nullptr)
            ;

        // Expect an error due absolute paths in the archive
        ASSERT_TRUE(archiveReader.hasError());
        // std::cerr << "Error: " << archiveReader.error().what() << std::endl;
    }
}

TEST(LibarchiveFileReaderTest, TestFilesInSymlinkCauseFailure)
{
    std::vector<std::filesystem::path> testPaths = { TEST_DATA_DIR "/archives/nested_symlinks.tar",
                                                     TEST_DATA_DIR "/archives/nested_symlinks.zip" };

    for (const auto &testPath : testPaths)
    {
        int fd = open(testPath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(fd, 0) << "Failed to open test archive file " << testPath << " - " << strerror(errno);

        struct stat st = {};
        ASSERT_EQ(fstat(fd, &st), 0) << "Failed to stat test archive file " << testPath << " - " << strerror(errno);
        ASSERT_GT(st.st_size, 0) << "Test archive file is empty";

        Error err;
        auto reader = std::make_unique<LibarchiveFileReader>(fd, st.st_size, ArchiveFormats::All, true, &err);
        ASSERT_NE(reader, nullptr);
        ASSERT_FALSE(reader->isNull()) << err.what();

        ArchiveReader archiveReader(std::move(reader));
        ASSERT_FALSE(archiveReader.hasError()) << archiveReader.error().what();

        std::unique_ptr<IPackageEntryImpl> entry;
        while ((entry = archiveReader.next()) != nullptr)
            ;

        // Expect an error due to files in symlinks
        ASSERT_TRUE(archiveReader.hasError());
        // std::cerr << "Error: " << archiveReader.error().what() << std::endl;
    }
}