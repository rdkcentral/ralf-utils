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

#include "dm-verity/IDmVerityProtectedFile.h"
#include "utils/FileUtils.h"
#include "utils/ShaUtils.h"
#include "utils/StringUtils.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <list>
#include <random>

#if defined(__linux__)
#    include <fcntl.h>
#    include <unistd.h>
#endif

using namespace ::testing;
using namespace entos::ralf::dmverity;

#define BLOCK_SIZE 4096u

/// This is not a real test, it is just a check to see if the test data is present and the correct size.
/// A common test failure for first time users is that they haven't checked out the code with git LFS enabled
/// meaning the test data is missing or the wrong size.
TEST(DmVerityProtectedFile, checkTestDataSize)
{
    const std::filesystem::path testDataDir = TEST_DATA_DIR;
    ASSERT_TRUE(std::filesystem::exists(testDataDir / "partial/concatenated.img"))
        << "Test data not found - have you run `git lfs install` and `git lfs pull`?";
    ASSERT_EQ(std::filesystem::file_size(testDataDir / "partial/concatenated.img"), 1064960)
        << "Invalid size of testdata - have you run `git lfs install` and `git lfs pull`?";
}

TEST(DmVerityProtectedFile, testHappyPath)
{
    std::list<std::filesystem::path> baseDirs = {
        TEST_DATA_DIR "/1block",
        TEST_DATA_DIR "/2block",
    };

    for (const auto &baseDir : baseDirs)
    {
        auto rootHash = fromHex(fileStrContents(baseDir / "roothash.txt"));
        ASSERT_EQ(rootHash.size(), 32u);

        auto result = IDmVerityProtectedFile::open(baseDir / "data.img", std::nullopt, std::nullopt,
                                                   baseDir / "hashes.img", std::nullopt, std::nullopt, rootHash);
        ASSERT_TRUE(result.hasValue()) << "failed to open dm-verity protected file: " << result.error().what();

        auto file = std::move(result.value());
        ASSERT_NE(file, nullptr);

        const size_t fileSize = std::filesystem::file_size(baseDir / "data.img");
        EXPECT_EQ(file->size(), fileSize);

        // Verify data blocks
        std::vector<uint8_t> block(BLOCK_SIZE);
        for (size_t n = 0; n < fileSize; n += block.size())
        {
            const size_t amount = std::min(block.size(), (fileSize - n));
            EXPECT_TRUE(file->read(block.data(), amount, n));
        }
    }
}

TEST(DmVerityProtectedFile, testInvalidFileRanges)
{
    const std::filesystem::path basePath = TEST_DATA_DIR "/2block/";

    auto rootHash = fromHex(fileStrContents(basePath / "roothash.txt"));
    ASSERT_EQ(rootHash.size(), 32u);

    auto file =
        IDmVerityProtectedFile::open(basePath / "data.img", 0, 8192, basePath / "hashes.img", 0, 8192 + 3, rootHash);
    EXPECT_FALSE(file);

    file = IDmVerityProtectedFile::open(basePath / "data.img", 0, 8192, basePath / "hashes.img", 3, 8192, rootHash);
    EXPECT_FALSE(file);

    file = IDmVerityProtectedFile::open(basePath / "data.img", 0, 8192, basePath / "hashes.img", 3, 4096, rootHash);
    EXPECT_FALSE(file);

    file = IDmVerityProtectedFile::open(basePath / "data.img", 0, 8192 + 6, basePath / "hashes.img", 0, 8192, rootHash);
    EXPECT_FALSE(file);

    file = IDmVerityProtectedFile::open(basePath / "data.img", 5, 8192, basePath / "hashes.img", 0, 8192, rootHash);
    EXPECT_FALSE(file);
}

TEST(DmVerityProtectedFile, testInvalidRootHash)
{
    const std::filesystem::path basePath = TEST_DATA_DIR "/2block/";

    auto rootHash = fromHex(fileStrContents(basePath / "roothash.txt"));
    ASSERT_EQ(rootHash.size(), 32u);

    rootHash[3] = ~rootHash[3];

    auto file = IDmVerityProtectedFile::open(basePath / "data.img", std::nullopt, std::nullopt, basePath / "hashes.img",
                                             std::nullopt, std::nullopt, rootHash);
    EXPECT_TRUE(!file);
}

TEST(DmVerityProtectedFile, testCorruptedFile)
{
    const std::filesystem::path basePath = TEST_DATA_DIR "/2block/";

    auto rootHash = fromHex(fileStrContents(basePath / "roothash.txt"));
    ASSERT_EQ(rootHash.size(), 32u);

    auto result = IDmVerityProtectedFile::open(basePath / "data.corrupted.img", std::nullopt, std::nullopt,
                                               basePath / "hashes.img", std::nullopt, std::nullopt, rootHash);
    ASSERT_TRUE(result.hasValue()) << "failed to open dm-verity protected file: " << result.error().what();

    auto file = std::move(result.value());
    ASSERT_NE(file, nullptr);

    std::vector<uint8_t> block(BLOCK_SIZE);

    // verify data blocks - first block was corrupted, should fail, the other should be fine
    EXPECT_FALSE(file->read(block.data(), block.size(), 0));
    EXPECT_TRUE(file->read(block.data(), block.size(), BLOCK_SIZE));
}

TEST(DmVerityProtectedFile, testCorruptedHashes)
{
    const std::filesystem::path basePath = TEST_DATA_DIR "/2block/";

    auto rootHash = fromHex(fileStrContents(basePath / "roothash.txt"));
    ASSERT_EQ(rootHash.size(), 32u);

    auto result = IDmVerityProtectedFile::open(basePath / "data.img", std::nullopt, std::nullopt,
                                               basePath / "hashes.corrupted.img", std::nullopt, std::nullopt, rootHash);
    EXPECT_TRUE(result.isError());
}

TEST(DmVerityProtectedFile, testPartialProtection)
{
    // The data file in resources/partial/data.img is 1MB in size, but the hash tree only covers 928K of the file
    const std::filesystem::path basePath = TEST_DATA_DIR "/partial/";

    auto rootHash = fromHex(fileStrContents(basePath / "roothash.txt"));
    ASSERT_EQ(rootHash.size(), 32u);

    auto result = IDmVerityProtectedFile::open(basePath / "data.img", std::nullopt, std::nullopt,
                                               basePath / "hashes.img", std::nullopt, std::nullopt, rootHash);
    EXPECT_TRUE(result.isError());

    // Should succeed if give correct data range
    result = IDmVerityProtectedFile::open(basePath / "data.img", 0, (928 * 1024), basePath / "hashes.img", std::nullopt,
                                          std::nullopt, rootHash);
    EXPECT_FALSE(result.isError());

    // Should also succeed if the data size is less than the hash tree
    result = IDmVerityProtectedFile::open(basePath / "data.img", 0, (927 * 1024), basePath / "hashes.img", std::nullopt,
                                          std::nullopt, rootHash);
    EXPECT_FALSE(result.isError());
}

TEST(DmVerityProtectedFile, testRandomRead)
{
    std::list<std::pair<std::filesystem::path, std::optional<size_t>>> baseDirs = {
        { TEST_DATA_DIR "/1block", std::nullopt },
        { TEST_DATA_DIR "/2block", std::nullopt },
        { TEST_DATA_DIR "/partial", (928 * 1024) },
    };

    std::random_device rd;

    for (const auto &entry : baseDirs)
    {
        const auto &baseDir = entry.first;
        const auto &dataSize = entry.second.value_or(std::filesystem::file_size(baseDir / "data.img"));

        auto rootHash = fromHex(fileStrContents(baseDir / "roothash.txt"));
        ASSERT_EQ(rootHash.size(), 32u);

        auto result = IDmVerityProtectedFile::open(baseDir / "data.img", 0, dataSize, baseDir / "hashes.img",
                                                   std::nullopt, std::nullopt, rootHash);
        ASSERT_TRUE(result.hasValue()) << "failed to open dm-verity protected file: " << result.error().what();

        auto file = std::move(result.value());
        ASSERT_NE(file, nullptr);
        EXPECT_EQ(file->size(), dataSize);

        const unsigned int seed = rd();
        std::mt19937 mt(seed);

        // Split the file into random byte ranges
        size_t offset = 0;
        struct Range
        {
            size_t offset;
            size_t size;
        };
        std::deque<Range> ranges;
        while (offset < dataSize)
        {
            std::uniform_int_distribution<size_t> dist(0, std::min<size_t>(dataSize - offset, 128));
            const size_t bytes = dist(mt);

            ranges.emplace_back(Range{ offset, bytes });
            offset += bytes;
        }

        // Shuffle the ranges
        std::shuffle(ranges.begin(), ranges.end(), mt);

        // Read each random range and put in a vector for the end
        std::unique_ptr<uint8_t[]> content(new uint8_t[dataSize]);
        for (const auto &range : ranges)
        {
            EXPECT_TRUE(file->read(content.get() + range.offset, range.size, range.offset))
                << "verified read failed for " << (baseDir / "data.img") << " when using random seed " << seed;
        }

        // Verify the read file matches
        EXPECT_EQ(sha256Sum(content.get(), dataSize), fileSha256(baseDir / "data.img", 0, dataSize))
            << "sha256 wrong for " << (baseDir / "data.img") << " when using random seed " << seed;
    }
}

TEST(DmVerityProtectedFile, testConcatenatedFiles)
{
    struct ImgDetails
    {
        std::filesystem::path baseDir;
        size_t dataOffset = 0;
        size_t dataSize = 0;
        size_t hashesOffset = 0;
        size_t hashesSize = 0;
    };

    std::list<ImgDetails> details = {
        { TEST_DATA_DIR "/1block", 0, (1 * BLOCK_SIZE), (1 * BLOCK_SIZE), (1 * BLOCK_SIZE) },
        { TEST_DATA_DIR "/2block", 0, (2 * BLOCK_SIZE), (2 * BLOCK_SIZE), (2 * BLOCK_SIZE) },
        { TEST_DATA_DIR "/partial", 0, (928 * 1024), (1024 * 1024), (4 * BLOCK_SIZE) },
    };

    std::random_device rd;

    for (const auto &dets : details)
    {
        const auto &baseDir = dets.baseDir;

        auto rootHash = fromHex(fileStrContents(baseDir / "roothash.txt"));
        ASSERT_EQ(rootHash.size(), 32u);

        auto result = IDmVerityProtectedFile::open(baseDir / "concatenated.img", dets.dataOffset, dets.dataSize,
                                                   baseDir / "concatenated.img", dets.hashesOffset, dets.hashesSize,
                                                   rootHash);
        ASSERT_TRUE(result.hasValue()) << "failed to open dm-verity protected file: " << result.error().what();

        auto file = std::move(result.value());
        ASSERT_NE(file, nullptr);
        EXPECT_EQ(file->size(), dets.dataSize);

        const unsigned int seed = rd();
        std::mt19937 mt(seed);

        // Split the file into random byte ranges
        size_t offset = 0;
        struct Range
        {
            size_t offset;
            size_t size;
        };
        std::deque<Range> ranges;
        while (offset < dets.dataSize)
        {
            std::uniform_int_distribution<size_t> dist(0, std::min<size_t>(dets.dataSize - offset, 128));
            const size_t bytes = dist(mt);

            ranges.emplace_back(Range{ offset, bytes });
            offset += bytes;
        }

        // Shuffle the ranges
        std::shuffle(ranges.begin(), ranges.end(), mt);

        // read each random range and put in a vector for the end
        std::unique_ptr<uint8_t[]> content(new uint8_t[dets.dataSize]);
        for (const auto &range : ranges)
        {
            EXPECT_TRUE(file->read(content.get() + range.offset, range.size, range.offset))
                << "verified read failed for " << (baseDir / "concatenated.img") << " when using random seed " << seed;
        }

        // Verify the read file matches
        EXPECT_EQ(sha256Sum(content.get(), dets.dataSize),
                  fileSha256(baseDir / "concatenated.img", dets.dataOffset, dets.dataSize))
            << "sha256 wrong for " << (baseDir / "concatenated.img") << " when using random seed " << seed;
    }
}

#if defined(__linux__)
TEST(DmVerityProtectedFile, testDirectIORead)
{
    // Check if O_DIRECT is supported
    {
        int fd = open(TEST_DATA_DIR "/1block/concatenated.img", O_DIRECT | O_RDONLY | O_CLOEXEC);
        if (fd < 0)
        {
            if (errno == EINVAL)
                GTEST_SKIP() << "O_DIRECT not supported on this filesystem, skipping test";
            else
                FAIL() << "failed to open concatenated image file: " << strerror(errno);
        }
        close(fd);
    }

    struct ImgDetails
    {
        std::filesystem::path baseDir;
        size_t dataOffset = 0;
        size_t dataSize = 0;
        size_t hashesOffset = 0;
        size_t hashesSize = 0;
    };

    std::list<ImgDetails> details = {
        { TEST_DATA_DIR "/1block", 0, (1 * BLOCK_SIZE), (1 * BLOCK_SIZE), (1 * BLOCK_SIZE) },
        { TEST_DATA_DIR "/2block", 0, (2 * BLOCK_SIZE), (2 * BLOCK_SIZE), (2 * BLOCK_SIZE) },
        { TEST_DATA_DIR "/partial", 0, (928 * 1024), (1024 * 1024), (4 * BLOCK_SIZE) },
    };

    std::random_device rd;

    for (const auto &dets : details)
    {
        const auto &baseDir = dets.baseDir;

        auto rootHash = fromHex(fileStrContents(baseDir / "roothash.txt"));
        ASSERT_EQ(rootHash.size(), 32u);

        int fd = open((baseDir / "concatenated.img").c_str(), O_DIRECT | O_RDONLY | O_CLOEXEC);
        ASSERT_GE(fd, 0) << "failed to open concatenated image file: " << strerror(errno);

        auto result = IDmVerityProtectedFile::open(fd, dets.dataOffset, dets.dataSize, fd, dets.hashesOffset,
                                                   dets.hashesSize, rootHash);
        ASSERT_TRUE(result.hasValue()) << "failed to open dm-verity protected file: " << result.error().what();

        ASSERT_EQ(close(fd), 0);

        auto file = std::move(result.value());
        ASSERT_NE(file, nullptr);
        EXPECT_EQ(file->size(), dets.dataSize);

        const unsigned int seed = rd();
        std::mt19937 mt(seed);

        // Split the file into random byte ranges
        size_t offset = 0;
        struct Range
        {
            size_t offset;
            size_t size;
        };
        std::deque<Range> ranges;
        while (offset < dets.dataSize)
        {
            std::uniform_int_distribution<size_t> dist(0, std::min<size_t>(dets.dataSize - offset, 128));
            const size_t bytes = dist(mt);

            ranges.emplace_back(Range{ offset, bytes });
            offset += bytes;
        }

        // Shuffle the ranges
        std::shuffle(ranges.begin(), ranges.end(), mt);

        // read each random range and put in a vector for the end
        std::unique_ptr<uint8_t[]> content(new uint8_t[dets.dataSize]);
        for (const auto &range : ranges)
        {
            EXPECT_TRUE(file->read(content.get() + range.offset, range.size, range.offset))
                << "verified read failed for " << (baseDir / "concatenated.img") << " when using random seed " << seed;
        }

        // Verify the read file matches
        EXPECT_EQ(sha256Sum(content.get(), dets.dataSize),
                  fileSha256(baseDir / "concatenated.img", dets.dataOffset, dets.dataSize))
            << "sha256 wrong for " << (baseDir / "concatenated.img") << " when using random seed " << seed;
    }
}
#endif // defined(__linux__)
