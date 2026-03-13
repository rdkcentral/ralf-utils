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

#include "dm-verity/IDmVerityVerifier.h"
#include "utils/FileUtils.h"
#include "utils/StringUtils.h"

#include <gtest/gtest.h>

#include <list>

using namespace ::testing;
using namespace entos::ralf::dmverity;

#define BLOCK_SIZE 4096u

TEST(DmVerityVerifier, testHappyPath)
{
    std::list<std::filesystem::path> baseDirs = { TEST_DATA_DIR "/1block", TEST_DATA_DIR "/2block",
                                                  TEST_DATA_DIR "/partial" };

    for (const auto &baseDir : baseDirs)
    {
        auto rootHash = fromHex(fileStrContents(baseDir / "roothash.txt"));
        ASSERT_EQ(rootHash.size(), 32u);

        auto result = IDmVerityVerifier::fromHashesFile(baseDir / "hashes.img", rootHash, std::nullopt, std::nullopt);
        ASSERT_TRUE(result.hasValue()) << "Failed to create verifier: " << result.error().what();

        auto &verifier = result.value();
        ASSERT_NE(verifier, nullptr);

        EXPECT_EQ(verifier->blockSize(), BLOCK_SIZE);
        EXPECT_GE(verifier->dataBlockCount(), 1u);

        // verify data blocks
        for (size_t n = 0; n < verifier->dataBlockCount(); n++)
        {
            auto block = fileContents(baseDir / "data.img", (n * BLOCK_SIZE), BLOCK_SIZE);
            ASSERT_EQ(block.size(), BLOCK_SIZE);

            EXPECT_TRUE(verifier->verify(n, block.data()));
        }
    }
}

TEST(DmVerityVerifier, testInvalidRootHash)
{
    const std::filesystem::path basePath = TEST_DATA_DIR "/2block/";

    auto rootHash = fromHex(fileStrContents(basePath / "roothash.txt"));
    ASSERT_EQ(rootHash.size(), 32u);

    rootHash[3] = ~rootHash[3];

    auto result = IDmVerityVerifier::fromHashesFile(basePath / "hashes.img", rootHash, std::nullopt, std::nullopt);
    EXPECT_TRUE(result.isError());
}

TEST(DmVerityVerifier, testCorruptedFile)
{
    const std::filesystem::path basePath = TEST_DATA_DIR "/2block/";

    auto rootHash = fromHex(fileStrContents(basePath / "roothash.txt"));
    ASSERT_EQ(rootHash.size(), 32u);

    auto result = IDmVerityVerifier::fromHashesFile(basePath / "hashes.img", rootHash, std::nullopt, std::nullopt);
    ASSERT_TRUE(result.hasValue()) << "Failed to create verifier: " << result.error().what();

    auto &verifier = result.value();
    ASSERT_NE(verifier, nullptr);

    // verify data blocks - first block was corrupted, should fail, the other should be fine
    auto block = fileContents(basePath / "data.corrupted.img", 0, BLOCK_SIZE);
    EXPECT_FALSE(verifier->verify(0, block.data()));

    block = fileContents(basePath / "data.corrupted.img", BLOCK_SIZE, BLOCK_SIZE);
    EXPECT_TRUE(verifier->verify(1, block.data()));
}

TEST(DmVerityVerifier, testCorruptedHashes)
{
    const std::filesystem::path basePath = TEST_DATA_DIR "/2block/";

    auto rootHash = fromHex(fileStrContents(basePath / "roothash.txt"));
    ASSERT_EQ(rootHash.size(), 32u);

    auto result =
        IDmVerityVerifier::fromHashesFile(basePath / "hashes.corrupted.img", rootHash, std::nullopt, std::nullopt);
    EXPECT_TRUE(result.isError());
}

TEST(DmVerityVerifier, testPartialProtection)
{
    // The data file in resources/partial/data.img is 1MB in size, but the hash tree only covers 928K of the file
    const std::filesystem::path basePath = TEST_DATA_DIR "/partial/";

    auto rootHash = fromHex(fileStrContents(basePath / "roothash.txt"));
    ASSERT_EQ(rootHash.size(), 32u);

    auto result = IDmVerityVerifier::fromHashesFile(basePath / "hashes.img", rootHash, std::nullopt, std::nullopt);
    ASSERT_TRUE(result.hasValue()) << "Failed to create verifier: " << result.error().what();

    auto &verifier = result.value();
    ASSERT_NE(verifier, nullptr);

    EXPECT_EQ(verifier->blockSize(), BLOCK_SIZE);
    EXPECT_EQ(verifier->dataBlockCount(), 232u);

    // Verify protected data blocks
    for (size_t n = 0; n < verifier->dataBlockCount(); n++)
    {
        auto block = fileContents(basePath / "data.img", (n * BLOCK_SIZE), BLOCK_SIZE);
        ASSERT_EQ(block.size(), BLOCK_SIZE);

        EXPECT_TRUE(verifier->verify(n, block.data()));
    }

    // Verify the rest of the file should fail
    for (size_t n = verifier->dataBlockCount(); n < 256; n++)
    {
        auto block = fileContents(basePath / "data.img", (n * BLOCK_SIZE), BLOCK_SIZE);
        ASSERT_EQ(block.size(), BLOCK_SIZE);

        EXPECT_FALSE(verifier->verify(n, block.data()));
    }
}
