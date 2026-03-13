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

#include "dm-verity/DmVeritySuperBlock.h"

#include <gtest/gtest.h>

#include <list>

using namespace ::testing;
using namespace entos::ralf::dmverity;

static const VeritySuperBlock goodSuperBlock = { .signature = { 'v', 'e', 'r', 'i', 't', 'y', '\0', '\0' },
                                                 .version = 1,
                                                 .hashType = 1,
                                                 .uuid = {},
                                                 .algorithm = { 's', 'h', 'a', '2', '5', '6', '\0' },
                                                 .dataBlockSize = 4096,
                                                 .hashBlockSize = 4096,
                                                 .dataBlocks = 0,
                                                 .saltSize = 32,
                                                 .salt = {} };

TEST(DmVeritySuperBlock, testGoodSuperBlock)
{
    EXPECT_TRUE(checkSuperBlock(&goodSuperBlock));
}

TEST(DmVeritySuperBlock, testBadSignature)
{
    const std::list<std::array<uint8_t, 8>> badSignatures = {
        { 'v', 'e', 'r', 'i', 't', 'y', '\0', '1' },
        { '\0', 'e', 'r', 'i', 't', 'y', '\0', '1' },
        { 'V', 'e', 'r', 'i', 't', 'y', '\0', '1' },
        { 'V', 'e', 'r', '\0', 't', 'y', '\0', '\0' },
    };

    for (const auto badSignature : badSignatures)
    {
        VeritySuperBlock sb = goodSuperBlock;
        memcpy(sb.signature, badSignature.data(), badSignature.size());

        EXPECT_FALSE(checkSuperBlock(&sb));
    }
}

TEST(DmVeritySuperBlock, testBadVersion)
{
    VeritySuperBlock sb = goodSuperBlock;
    sb.version = 123;

    EXPECT_FALSE(checkSuperBlock(&sb));
}

TEST(DmVeritySuperBlock, testBadType)
{
    VeritySuperBlock sb = goodSuperBlock;
    sb.hashType = 0;

    EXPECT_FALSE(checkSuperBlock(&sb));
}

TEST(DmVeritySuperBlock, testBadAlgorithm)
{
    const std::list<std::array<uint8_t, 8>> badAlgorithms = {
        { 's', 'h', 'a', '2', '\0', '6', '\0', '\0' },  { 's', 'h', 'a', '2', '\0', '\0', '\0', '\0' },
        { 's', 'h', 'a', '2', '\0', '\0', '\0', '\0' }, { 's', 'h', 'a', '2', '5', '6', '7', '\0' },
        { 'S', 'h', 'a', '2', '5', '6', '\0', '\0' },   { '\0', 'h', 'a', '2', '5', '6', '\0', '\0' },
    };

    for (const auto badAlgorithm : badAlgorithms)
    {
        VeritySuperBlock sb = goodSuperBlock;
        memcpy(sb.algorithm, badAlgorithm.data(), badAlgorithm.size());

        EXPECT_FALSE(checkSuperBlock(&sb));
    }
}

TEST(DmVeritySuperBlock, testUnsupportedBlockSizes)
{
    VeritySuperBlock sb = {};

    sb = goodSuperBlock;
    sb.hashBlockSize = 8192;
    EXPECT_FALSE(checkSuperBlock(&sb));

    sb = goodSuperBlock;
    sb.dataBlockSize = 0;
    EXPECT_FALSE(checkSuperBlock(&sb));

    sb = goodSuperBlock;
    sb.dataBlockSize = 4095;
    EXPECT_FALSE(checkSuperBlock(&sb));
}

TEST(DmVeritySuperBlock, testInvalidSaltSize)
{
    VeritySuperBlock sb = {};

    sb = goodSuperBlock;
    sb.saltSize = 257;
    EXPECT_FALSE(checkSuperBlock(&sb));

    sb = goodSuperBlock;
    sb.saltSize = UINT16_MAX;
    EXPECT_FALSE(checkSuperBlock(&sb));
}
