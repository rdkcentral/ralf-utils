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

#include "erofs/IErofsReader.h"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <random>
#include <vector>

using namespace ::testing;
using namespace entos::ralf::erofs;

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using Sha256Sum = std::array<uint8_t, 32>;

struct ExpectedEntry
{
    std::filesystem::file_type type;
    std::filesystem::perms perms;
    time_t mtime;
    uid_t uid;
    gid_t gid;
    size_t size;
    Sha256Sum sha256;
};

using ExpectedFileMap = std::map<std::filesystem::path, ExpectedEntry>;

static const std::filesystem::path testDataDir = TEST_DATA_DIR;

// -----------------------------------------------------------------------------
/*!
    \internal

    Pretty printer for std::filesystem::perms and Sha256Sum types.

    \see https://github.com/google/googletest/discussions/4121
 */
namespace testing::internal
{
    template <>
    class UniversalPrinter<std::filesystem::perms>
    {
    public:
        static void Print(const std::filesystem::perms &perms, ::std::ostream *os) // NOLINT(readability-identifier-naming)
        {
            *os << '-';
            *os << (((perms & std::filesystem::perms::owner_read) == std::filesystem::perms::owner_read) ? 'r' : '-');
            *os << (((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::owner_write) ? 'w' : '-');
            *os << (((perms & std::filesystem::perms::owner_exec) == std::filesystem::perms::owner_exec) ? 'x' : '-');

            *os << (((perms & std::filesystem::perms::group_read) == std::filesystem::perms::group_read) ? 'r' : '-');
            *os << (((perms & std::filesystem::perms::group_write) == std::filesystem::perms::group_write) ? 'w' : '-');
            *os << (((perms & std::filesystem::perms::group_exec) == std::filesystem::perms::group_exec) ? 'x' : '-');

            *os << (((perms & std::filesystem::perms::others_read) == std::filesystem::perms::others_read) ? 'r' : '-');
            *os << (((perms & std::filesystem::perms::others_write) == std::filesystem::perms::others_write) ? 'w' : '-');
            *os << (((perms & std::filesystem::perms::others_exec) == std::filesystem::perms::others_exec) ? 'x' : '-');
        }
    };

    template <>
    class UniversalPrinter<Sha256Sum>
    {
    public:
        static void Print(const Sha256Sum &digest, ::std::ostream *os) // NOLINT(readability-identifier-naming)
        {
            static const char table[] = "0123456789abcdef";
            for (const uint8_t &b : digest)
            {
                *os << table[(b >> 4) & 0xf] << table[(b >> 0) & 0xf];
            }
        }
    };

} // namespace testing::internal

// -----------------------------------------------------------------------------
/*!
    \internal

    Converts a mode_t to std::filesystem::perms
 */
static std::filesystem::perms modeToPerms(mode_t mode)
{
    std::filesystem::perms perms = std::filesystem::perms::none;

    if (mode & S_IXOTH)
        perms |= std::filesystem::perms::others_exec;
    if (mode & S_IWOTH)
        perms |= std::filesystem::perms::others_write;
    if (mode & S_IROTH)
        perms |= std::filesystem::perms::others_read;

    if (mode & S_IXGRP)
        perms |= std::filesystem::perms::group_exec;
    if (mode & S_IWGRP)
        perms |= std::filesystem::perms::group_write;
    if (mode & S_IRGRP)
        perms |= std::filesystem::perms::group_read;

    if (mode & S_IXUSR)
        perms |= std::filesystem::perms::owner_exec;
    if (mode & S_IWUSR)
        perms |= std::filesystem::perms::owner_write;
    if (mode & S_IRUSR)
        perms |= std::filesystem::perms::owner_read;

    return perms;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Utility to convert a hex string to a 32-byte array.
 */
Sha256Sum parseSha256(const std::string &str)
{
    EXPECT_GE(str.length(), 64u) << "hex string too short";

    Sha256Sum data;

    unsigned i = 0;
    for (const char &ch : str)
    {
        uint8_t nibble;
        if ((ch >= '0') && (ch <= '9'))
        {
            nibble = ch - '0';
        }
        else if ((ch >= 'A') && (ch <= 'F'))
        {
            nibble = 0xa + (ch - 'A');
        }
        else if ((ch >= 'a') && (ch <= 'f'))
        {
            nibble = 0xa + (ch - 'a');
        }
        else
        {
            EXPECT_TRUE(false) << "invalid character in hex string";
            return {};
        }

        if (i & 0x1)
            data[i / 2] |= nibble;
        else
            data[i / 2] = (nibble << 4);

        if (++i >= 64)
            break;
    }

    return data;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Parses the json file to get the list of expected file, directories, symlinks
    etc in the filesystem.
 */
static ExpectedFileMap parseExpectedJson(const std::filesystem::path &jsonFile)
{
    ExpectedFileMap expectedMap;

    std::ifstream file;
    file.open(jsonFile, std::ifstream::in);
    EXPECT_TRUE(file.is_open());

    const auto root = nlohmann::json::parse(file);
    EXPECT_TRUE(root.is_object());

    for (const auto &[key, value] : root.items())
    {
        std::filesystem::path path = key;

        const uid_t uid = value["uid"].get<uid_t>();
        const gid_t gid = value["gid"].get<gid_t>();
        const mode_t mode = value["mode"].get<mode_t>();
        const time_t mtime = value["mtime"].get<time_t>();

        if (S_ISDIR(mode))
        {
            expectedMap.emplace(std::move(path),
                                ExpectedEntry{
                                    std::filesystem::file_type::directory, modeToPerms(mode), mtime, uid, gid, 0, {} });
        }
        else if (S_ISREG(mode))
        {
            expectedMap.emplace(std::move(path), ExpectedEntry{ std::filesystem::file_type::regular, modeToPerms(mode),
                                                                mtime, uid, gid, value["size"].get<size_t>(),
                                                                parseSha256(value["sha256"].get<std::string>()) });
        }
        else if (S_ISLNK(mode))
        {
            expectedMap.emplace(std::move(path), ExpectedEntry{ std::filesystem::file_type::symlink, modeToPerms(mode),
                                                                mtime, uid, gid, value["size"].get<size_t>(),
                                                                parseSha256(value["sha256"].get<std::string>()) });
        }
    }

    return expectedMap;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Calculates the SHA256 sum of the data stored in a ErofsEntry.
 */
static Sha256Sum sha256Sum(const std::unique_ptr<IErofsEntry> &entry)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    const size_t size = entry->size();
    size_t offset = 0;
    uint8_t buffer[4096];

    Error error;

    while (offset < size)
    {
        size_t amount = std::min(size - offset, sizeof(buffer));

        const auto result = entry->read(buffer, amount, offset, &error);
        EXPECT_GE(result, 0) << "error reading entry, result=" << result;
        EXPECT_FALSE(error) << "error reading entry: " << error.what();

        if (!result)
            return {};

        EVP_DigestUpdate(ctx, buffer, amount);

        offset += amount;
    }

    Sha256Sum digest;

    unsigned int digestLen = 32;
    EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char *>(digest.data()), &digestLen);
    EVP_MD_CTX_free(ctx);

    return digest;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Calculates the SHA256 sum of the raw data.
 */
static Sha256Sum sha256Sum(const uint8_t *data, size_t dataLen)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data, dataLen);

    Sha256Sum digest;

    unsigned int digestLen = 32;
    EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char *>(digest.data()), &digestLen);
    EVP_MD_CTX_free(ctx);

    return digest;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Runs the reader object over all files and compares with the provided
    expected map of files
 */
static void compareReaderVsExpected(const std::shared_ptr<IErofsReader> &reader, const std::filesystem::path &expectedJson)
{
    auto expected = parseExpectedJson(expectedJson);
    ASSERT_GT(expected.size(), 0u);

    while (reader->hasNext())
    {
        auto entry = reader->next();
        ASSERT_TRUE(!!entry);

        auto found = expected.find(entry->path());
        ASSERT_NE(found, expected.end()) << "failed to find entry " << entry->path();

        EXPECT_EQ(entry->type(), found->second.type) << "type of " << entry->path() << " is wrong";

        EXPECT_EQ(entry->permissions(), found->second.perms) << "perms on " << entry->path() << " are wrong";

        EXPECT_EQ(entry->modificationTime(), found->second.mtime) << "mtime on " << entry->path() << " is wrong";

        EXPECT_EQ(entry->ownerId(), found->second.uid) << "uid on " << entry->path() << " is wrong";

        EXPECT_EQ(entry->groupId(), found->second.gid) << "gid on " << entry->path() << " is wrong";

        EXPECT_EQ(entry->size(), found->second.size) << "size off " << entry->path() << " is wrong";

        if (entry->type() == std::filesystem::file_type::regular)
        {
            const auto digest = sha256Sum(entry);
            EXPECT_EQ(digest, found->second.sha256);
        }
        else if (entry->type() == std::filesystem::file_type::symlink)
        {
            const auto digest = sha256Sum(entry);
            EXPECT_EQ(digest, found->second.sha256);
        }

        expected.erase(found);
    }

    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->atEnd());

    // if map is empty then we found all entries
    EXPECT_EQ(expected.size(), 0u);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Given an inode it performs 'random' reads on it.  By random it means the
    read may be at a random offset with a random size.
 */
static std::unique_ptr<uint8_t[]> readEntryRandomly(int randomSeed, const std::unique_ptr<IErofsEntry> &entry)
{
    const size_t size = entry->size();
    size_t offset = 0;

    std::mt19937 mt(randomSeed);

    // spit the file into random byte ranges
    std::deque<std::pair<off_t, size_t>> ranges;
    while (offset < size)
    {
        std::uniform_int_distribution<size_t> dist(0, std::min<size_t>(size - offset, 128));
        const size_t bytes = dist(mt);

        ranges.emplace_back(offset, bytes);
        offset += bytes;
    }

    // shuffle the ranges
    std::shuffle(ranges.begin(), ranges.end(), mt);

    Error error;

    // read each random range and put in a vector for the end
    std::unique_ptr<uint8_t[]> content(new uint8_t[size]);
    for (const auto &range : ranges)
    {
        //fprintf(stderr, "reading range %6ld -> %6ld : size %zu\n",
        //        static_cast<long>(range.first),
        //        static_cast<long>(range.first + range.second),
        //        range.second);

        EXPECT_EQ(entry->read(content.get() + range.first, range.second, range.first, &error),
                  static_cast<ssize_t>(range.second))
            << "error reading entry at offset " << range.first << " and size " << range.second << " - " << error.what();
    }

    return content;
}

TEST(TestErofsReader, testValidButEmptyImage)
{
    auto result = IErofsReader::create(testDataDir / "erofs_empty.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    EXPECT_FALSE(reader->hasError());
    EXPECT_FALSE(reader->hasNext());
    EXPECT_TRUE(reader->atEnd());
}

TEST(TestErofsReader, testValidSimpleImage)
{
    auto result = IErofsReader::create(testDataDir / "erofs_1.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    compareReaderVsExpected(reader, testDataDir / "erofs_1.json");
}

TEST(TestErofsReader, testSuperCompressedFiles)
{
    auto result = IErofsReader::create(testDataDir / "erofs_2.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    compareReaderVsExpected(reader, testDataDir / "erofs_2.json");
}

TEST(TestErofsReader, testLegacyFormat)
{
    auto result = IErofsReader::create(testDataDir / "erofs_legacy.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    compareReaderVsExpected(reader, testDataDir / "erofs_legacy.json");
}

TEST(TestErofsReader, testModificationTimes)
{
    auto result = IErofsReader::create(testDataDir / "erofs_mtimes.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    compareReaderVsExpected(reader, testDataDir / "erofs_mtimes.json");
}

TEST(TestErofsReader, testUncompressedImage)
{
    auto result = IErofsReader::create(testDataDir / "erofs_uncompr.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    compareReaderVsExpected(reader, testDataDir / "erofs_uncompr.json");
}

TEST(TestErofsReader, testAdviceFlags)
{
    auto result = IErofsReader::create(testDataDir / "erofs_1.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    reader->setAdvice(IErofsReader::Advice::ReadAllOnce);

    ASSERT_TRUE(!!reader);
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    compareReaderVsExpected(reader, testDataDir / "erofs_1.json");
}

TEST(TestErofsReader, testRandomAccessReads)
{
    auto result = IErofsReader::create(testDataDir / "erofs_1.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    reader->setAdvice(IErofsReader::Advice::ReadAllOnce);

    ASSERT_TRUE(!!reader);
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    auto expected = parseExpectedJson(testDataDir / "erofs_1.json");
    ASSERT_GT(expected.size(), 0u);

    std::random_device rd;

    while (reader->hasNext())
    {
        auto entry = reader->next();
        ASSERT_TRUE(!!entry);

        auto found = expected.find(entry->path());
        ASSERT_NE(found, expected.end()) << "failed to find entry " << entry->path();

        if ((entry->type() == std::filesystem::file_type::regular) ||
            (entry->type() == std::filesystem::file_type::symlink))
        {
            const int seed = static_cast<int>(rd());
            SCOPED_TRACE(std::string("Random Seed: ") + std::to_string(seed));

            const auto content = readEntryRandomly(seed, entry);

            const auto digest = sha256Sum(content.get(), entry->size());
            ASSERT_EQ(digest, found->second.sha256)
                << "sha256 check failed on " << entry->path() << " when using random seed " << seed;
        }

        expected.erase(found);
    }

    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->atEnd());

    // if map is empty then we found all entries
    EXPECT_EQ(expected.size(), 0u);
}

TEST(TestErofsReader, testLargeNumberOfFiles)
{
    auto result = IErofsReader::create(testDataDir / "erofs_lotsoffiles.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    compareReaderVsExpected(reader, testDataDir / "erofs_lotsoffiles.json");
}

TEST(TestErofsReader, testBadRootDirectory)
{
    const auto result = IErofsReader::create(testDataDir / "erofs_bad_dirent_1.img");
    ASSERT_TRUE(result.isError());
}

TEST(TestErofsReader, testBadSubDirectory)
{
    auto result = IErofsReader::create(testDataDir / "erofs_bad_dirent_2.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    while (reader->hasNext())
    {
        reader->next();
    }

    EXPECT_TRUE(reader->hasError());
}

TEST(TestErofsReader, testBadDirentType)
{
    auto result = IErofsReader::create(testDataDir / "erofs_bad_dirent_3.img");
    ASSERT_TRUE(result.isError());
}

TEST(TestErofsReader, testBadFilename)
{
    auto result = IErofsReader::create(testDataDir / "erofs_bad_filename.img");
    ASSERT_TRUE(result.isError());
}

TEST(TestErofsReader, testUtfValidFilename)
{
    auto result = IErofsReader::create(testDataDir / "erofs_utf8_fnames.img");
    ASSERT_FALSE(result.isError()) << "failed to create reader - " << result.error().what();

    const auto &reader = result.value();
    EXPECT_FALSE(reader->hasError());
    EXPECT_TRUE(reader->hasNext());
    EXPECT_FALSE(reader->atEnd());

    compareReaderVsExpected(reader, testDataDir / "erofs_utf8_fnames.json");
}
