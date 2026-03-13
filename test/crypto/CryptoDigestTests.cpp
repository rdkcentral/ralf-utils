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

#include "StringUtils.h"
#include "core/CryptoDigestBuilder.h"

#include <gtest/gtest.h>

#include <map>
#include <vector>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

struct TestVector
{
    std::string data;
    size_t count;
    std::string digest;
};

// Test vectors from https://www.di-mgt.com.au/sha_testvectors.html
static const std::map<CryptoDigestBuilder::Algorithm, std::vector<TestVector>> testVectors = {
    { CryptoDigestBuilder::Algorithm::Md5,
      { { "", 1, "d41d8cd98f00b204e9800998ecf8427e" },
        { "abc", 1, "900150983cd24fb0d6963f7d28e17f72" },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890", 1,
          "57edf4a22be3c955ac49da2e2107b67a" },
        { "The quick brown fox jumps over the lazy dog", 1, "9e107d9d372bb6826bd81d3542a419d6" } } },

    { CryptoDigestBuilder::Algorithm::Sha1,
      { { "", 1, "da39a3ee5e6b4b0d3255bfef95601890afd80709" },
        { "abc", 1, "a9993e364706816aba3e25717850c26c9cd0d89d" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1, "84983e441c3bd26ebaae4aa1f95129e5e54670f1" },
        { "a", 1000000, "34aa973cd4c4daa4f61eeb2bdbad27316534016f" },
        { "0123456701234567012345670123456701234567012345670123456701234567", 10,
          "dea356a2cddd90c7a7ecedc5ebb563934f460452" } } },

    { CryptoDigestBuilder::Algorithm::Sha256,
      { { "", 1, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
        { "abc", 1, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
        { "a", 1000000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" },
        { "0123456701234567012345670123456701234567012345670123456701234567", 10,
          "594847328451bdfa85056225462cc1d867d877fb388df0ce35f25ab5562bfbb5" } } },

    { CryptoDigestBuilder::Algorithm::Sha384,
      { { "", 1, "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b" },
        { "abc", 1, "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
          "3391fdddfc8dc7393707a65b1b4709397cf8b1d162af05abfe8f450de5f36bc6b0455a8520bc4e6f5fe95b1fe3c8452b" },
        { "a", 1000000, "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985" },
        { "0123456701234567012345670123456701234567012345670123456701234567", 10,
          "2fc64a4f500ddb6828f6a3430b8dd72a368eb7f3a8322a70bc84275b9c0b3ab00d27a5cc3c2d224aa6b61a0d79fb4596" } } },

    { CryptoDigestBuilder::Algorithm::Sha512,
      { { "",
          1, "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e" },
        { "abc",
          1, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          1, "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c33596fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445" },
        { "a",
          1000000, "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973ebde0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b" },
        { "0123456701234567012345670123456701234567012345670123456701234567", 10,
          "89d05ba632c699c31231ded4ffc127d5a894dad412c0e024db872d1abd2ba8141a0f85072a9be1e2aa04cf33c765cb510813a39cd5a8"
          "4c4acaa64d3f3fb7bae9" } } },

};

TEST(CryptoDigestBuilderTest, TestAlgorithm)
{
    for (const auto &testVector : testVectors)
    {
        CryptoDigestBuilder builder(testVector.first);

        for (const auto &testData : testVector.second)
        {
            for (size_t i = 0; i < testData.count; i++)
                builder.update(testData.data.data(), testData.data.size());

            const auto expectedDigest = fromHex(testData.digest);
            const auto actualDigest = builder.finalise();
            EXPECT_EQ(actualDigest.size(), expectedDigest.size());
            EXPECT_EQ(actualDigest, expectedDigest)
                << "Algorithm: " << static_cast<int>(testVector.first) << ", Data: \"" << testData.data << "\"";

            builder.reset();
        }
    }
}

TEST(CryptoDigestBuilderTest, TestHelper)
{
    for (const auto &testVector : testVectors)
    {
        for (const auto &testData : testVector.second)
        {
            if (testData.count != 1)
                continue;

            const auto expectedDigest = fromHex(testData.digest);
            const auto actualDigest =
                CryptoDigestBuilder::digest(testVector.first, testData.data.data(), testData.data.size());
            EXPECT_EQ(actualDigest.size(), expectedDigest.size());
            EXPECT_EQ(actualDigest, expectedDigest)
                << "Algorithm: " << static_cast<int>(testVector.first) << ", Data: \"" << testData.data << "\"";
        }
    }
}

TEST(CryptoDigestBuilderTest, TestEmptyData)
{
    {
        CryptoDigestBuilder builder(CryptoDigestBuilder::Algorithm::Md5);

        auto digest = builder.finalise();
        EXPECT_EQ(digest.size(), 16);
        EXPECT_EQ(digest, fromHex("d41d8cd98f00b204e9800998ecf8427e"));
        ASSERT_EQ(digest, std::vector<uint8_t>({ 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98,
                                                 0xec, 0xf8, 0x42, 0x7e }));
    }

    {
        CryptoDigestBuilder builder(CryptoDigestBuilder::Algorithm::Sha1);

        auto digest = builder.finalise();
        ASSERT_EQ(digest.size(), 20);
        ASSERT_EQ(digest, std::vector<uint8_t>({ 0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
                                                 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09 }));
    }

    {
        CryptoDigestBuilder builder(CryptoDigestBuilder::Algorithm::Sha256);

        auto digest = builder.finalise();
        ASSERT_EQ(digest.size(), 32);
        ASSERT_EQ(digest, std::vector<uint8_t>({ 0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
                                                 0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
                                                 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55 }));
    }

    {
        CryptoDigestBuilder builder(CryptoDigestBuilder::Algorithm::Sha384);

        auto digest = builder.finalise();
        ASSERT_EQ(digest.size(), 48);
        ASSERT_EQ(digest,
                  std::vector<uint8_t>({ 0x38, 0xb0, 0x60, 0xa7, 0x51, 0xac, 0x96, 0x38, 0x4c, 0xd9, 0x32, 0x7e,
                                         0xb1, 0xb1, 0xe3, 0x6a, 0x21, 0xfd, 0xb7, 0x11, 0x14, 0xbe, 0x07, 0x43,
                                         0x4c, 0x0c, 0xc7, 0xbf, 0x63, 0xf6, 0xe1, 0xda, 0x27, 0x4e, 0xde, 0xbf,
                                         0xe7, 0x6f, 0x65, 0xfb, 0xd5, 0x1a, 0xd2, 0xf1, 0x48, 0x98, 0xb9, 0x5b }));
    }

    {
        CryptoDigestBuilder builder(CryptoDigestBuilder::Algorithm::Sha512);

        auto digest = builder.finalise();
        ASSERT_EQ(digest.size(), 64);
        ASSERT_EQ(digest,
                  std::vector<uint8_t>({ 0xcf, 0x83, 0xe1, 0x35, 0x7e, 0xef, 0xb8, 0xbd, 0xf1, 0x54, 0x28, 0x50, 0xd6,
                                         0x6d, 0x80, 0x07, 0xd6, 0x20, 0xe4, 0x05, 0x0b, 0x57, 0x15, 0xdc, 0x83, 0xf4,
                                         0xa9, 0x21, 0xd3, 0x6c, 0xe9, 0xce, 0x47, 0xd0, 0xd1, 0x3c, 0x5d, 0x85, 0xf2,
                                         0xb0, 0xff, 0x83, 0x18, 0xd2, 0x87, 0x7e, 0xec, 0x2f, 0x63, 0xb9, 0x31, 0xbd,
                                         0x47, 0x41, 0x7a, 0x81, 0xa5, 0x38, 0x32, 0x7a, 0xf9, 0x27, 0xda, 0x3e }));
    }
}

TEST(CryptoDigestBuilderTest, TestReset)
{
    for (const auto &testVector : testVectors)
    {
        for (const auto &testData : testVector.second)
        {
            CryptoDigestBuilder builder(testVector.first);

            builder.update(testData.data.data(), testData.data.size());
            builder.reset();

            for (size_t i = 0; i < testData.count; i++)
                builder.update(testData.data.data(), testData.data.size());

            const auto expectedDigest = fromHex(testData.digest);
            const auto actualDigest = builder.finalise();
            EXPECT_EQ(actualDigest.size(), expectedDigest.size());
            EXPECT_EQ(actualDigest, expectedDigest)
                << "Algorithm: " << static_cast<int>(testVector.first) << ", Data: \"" << testData.data << "\"";
        }
    }
}

TEST(CryptoDigestBuilderTest, TestMoveConstructor)
{
    for (const auto &testVector : testVectors)
    {
        for (const auto &testData : testVector.second)
        {
            CryptoDigestBuilder builder(testVector.first);

            for (size_t i = 0; i < testData.count; i++)
                builder.update(testData.data.data(), testData.data.size());

            CryptoDigestBuilder builder2(std::move(builder));

            const auto expectedDigest = fromHex(testData.digest);
            const auto actualDigest = builder2.finalise();
            EXPECT_EQ(actualDigest.size(), expectedDigest.size());
            EXPECT_EQ(actualDigest, expectedDigest)
                << "Algorithm: " << static_cast<int>(testVector.first) << ", Data: \"" << testData.data << "\"";
        }
    }
}

TEST(CryptoDigestBuilderTest, TestMoveOperator)
{
    for (const auto &testVector : testVectors)
    {
        for (const auto &testData : testVector.second)
        {
            CryptoDigestBuilder builder(testVector.first);

            for (size_t i = 0; i < testData.count; i++)
                builder.update(testData.data.data(), testData.data.size());

            CryptoDigestBuilder builder2 = std::move(builder);

            const auto expectedDigest = fromHex(testData.digest);
            const auto actualDigest = builder2.finalise();
            EXPECT_EQ(actualDigest.size(), expectedDigest.size());
            EXPECT_EQ(actualDigest, expectedDigest)
                << "Algorithm: " << static_cast<int>(testVector.first) << ", Data: \"" << testData.data << "\"";
        }
    }
}

template <class T, std::size_t N>
bool vectorSameAsArray(const std::vector<T> &v, const std::array<T, N> &a)
{
    if (v.size() != N)
        return false;

    return std::equal(v.begin(), v.end(), a.begin());
}

TEST(CryptoDigestBuilderTest, TestSha256Specialisation)
{
    // test standard data
    const auto &testData = testVectors.at(CryptoDigestBuilder::Algorithm::Sha256);
    for (const auto &data : testData)
    {
        CryptoDigestBuilder::sha256Digest(data.data.data(), data.data.size());
        if (data.count != 1)
            continue;

        const auto expectedDigest = fromHex(data.digest);
        const auto actualDigest = CryptoDigestBuilder::sha256Digest(data.data.data(), data.data.size());
        EXPECT_TRUE(vectorSameAsArray(expectedDigest, actualDigest));
    }

    {

        const auto digest = CryptoDigestBuilder::sha256Digest("password", 8, "salt", 4);
        const std::vector<uint8_t> expected =
            fromHex("13601bda4ea78e55a07b98866d2be6be0744e3866f13c00c811cab608a28f322");

        EXPECT_TRUE(vectorSameAsArray(expected, digest))
            << "Expected: " << toHex(expected) << ", Actual: " << toHex(digest);
    }
}

TEST(CryptoDigestBuilderTest, TestZeroData) {}