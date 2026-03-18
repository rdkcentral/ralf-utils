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

#include "../utils/StringUtils.h"
#include "MockDmVerityMounter.h"
#include "MockPackageMountImpl.h"
#include "oci/OCIArchiveBackingStore.h"
#include "oci/OCIPackage.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace ::testing;
using namespace entos::ralf::dmverity;

namespace entos::ralf::dmverity
{
    std::ostream &operator<<(std::ostream &os, const IDmVerityMounter::FileRange &range)
    {
        os << "FileRange(offset=" << range.offset << ", size=" << range.size << ")";
        return os;
    }
} // namespace entos::ralf::dmverity

static VerificationBundle createTestVerificationBundle()
{
    auto cert = Certificate::loadFromFile(TEST_DATA_DIR "/certs/rootCA.cert.pem", Certificate::EncodingFormat::PEM);
    EXPECT_TRUE(cert) << "failed to load rootCA.cert.pem - " << cert.error().what();

    VerificationBundle bundle;
    bundle.addCertificate(cert.value());

    return bundle;
}

TEST(PackageMountableTest, isMountable)
{
    struct TestCase
    {
        std::filesystem::path path;
        bool mountable;
    };

    const std::vector<TestCase> packages = {
        { TEST_DATA_DIR "/ralf-144mb.tar", true },     // tarball with EROFS image
        { TEST_DATA_DIR "/ralf-144mb.zip", true },     // zip with EROFS image
        { TEST_DATA_DIR "/ralf-144mb.tar.gz", false }, // gzipped tarball with EROFS image (not mountable, compressed)
        { TEST_DATA_DIR "/ralf-16k-files.tar", true }, // tarball with EROFS image
        { TEST_DATA_DIR "/ralf-simple.tar", false },   // tarball with non-EROFS image
        { TEST_DATA_DIR "/ralf-simple.zip", false },   // zip with non-EROFS image
        { TEST_DATA_DIR "/widget-144mb.wgt", false },  // traditional widget package, not mountable
    };

    const auto bundle = createTestVerificationBundle();

    for (const auto &test : packages)
    {
        int packageFd = open(test.path.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(packageFd, 0) << "failed to open " << test.path << " - " << strerror(errno);

        auto package = Package::open(packageFd, bundle, Package::OpenFlags::None);
        ASSERT_FALSE(package.isError()) << "failed to open " << test.path << " due to " << package.error().what();

        ASSERT_EQ(close(packageFd), 0);

        EXPECT_EQ(package->isMountable(), test.mountable) << "isMountable() wrong for " << test.path;

        close(packageFd);
    }
}

MATCHER_P(AreSameFile, fd2, "Checks if two file descriptors refer to the same file")
{
    struct stat stat1 = {}, stat2 = {};

    if (fstat(arg, &stat1) != 0 || fstat(fd2, &stat2) != 0)
        return false;

    return (stat1.st_dev == stat2.st_dev) && (stat1.st_ino == stat2.st_ino);
}

TEST(PackageMountableTest, canGetMappableImage)
{
    struct TestCase
    {
        std::filesystem::path path;
        bool expectMount;

        IDmVerityMounter::FileRange dataRange;
        IDmVerityMounter::FileRange hashesRange;
        std::vector<uint8_t> rootHash;
        std::vector<uint8_t> salt;
    };

    // The following packages were created by the ralfpack tool, in some cases a modified version, to remove the
    // alignment or compress the image file to test various scenarios.
    // The expected values were then gathered by using tools like zipdetails and hexdump to manually inspect the
    // contents of the packages.

    const std::vector<TestCase> packages = {
        // tarball with EROFS image
        {
            TEST_DATA_DIR "/ralf-144mb.tar",
            true,
            { 8192, 692224 },                                                            // data range
            { 8192 + 692224, 708608 - 692224 },                                          // hashes range
            fromHex("0a9126ef97efae39c4e2bc1c36fd3d62a75ba4609f65e614ca04794988946887"), // root hash
            fromHex("9471b91d716f4a874a4c24356145cb7042f094d7ef9770896f97b34ecb8d1f51")  // salt
        },

        // tarball with EROFS image but not aligned, so not mountable
        {
            TEST_DATA_DIR "/ralf-144mb-unaligned.tar",
            false,
            { 7680, 692224 },                                                            // data range
            { 7680 + 692224, 708608 - 692224 },                                          // hashes range
            fromHex("c76adbda23219d25b1d97192d101750adb5dde3d079f6f5417b6aae4e8caf2f9"), // root hash
            fromHex("0f8034163c3788081b35fa18b21a944e245353d31a4419ac9bbd2ec9b28039aa")  // salt
        },

        // zip with EROFS image aligned and uncompressed, so mountable
        {
            TEST_DATA_DIR "/ralf-144mb.zip",
            true,
            { 4096, 692224 },                                                            // data range
            { 4096 + 692224, 708608 - 692224 },                                          // hashes range
            fromHex("9bab783d4bd29e6e85a5f769c10c66f8b0dfb804983f1b162e728c1c80fbc61b"), // root hash
            fromHex("53024c31b68ddeb10a0379fa126cfafa70fdf5684cf85e32df33a6538cd326f1")  // salt
        },

        // zip with EROFS image, aligned but compressed, so not mountable
        {
            TEST_DATA_DIR "/ralf-144mb-compr-image.zip",
            false,
            { 4096, 692224 },                                                            // data range
            { 4096 + 692224, 708608 - 692224 },                                          // hashes range
            fromHex("27cad61bf6a1e31584e0698e4c3ae1fde6d9b4181c783dffd2213027a1463268"), // root hash
            fromHex("2514834e8661e0b1457f5541848e99643a872ab22e961e9f134503124534cdc4")  // salt
        },

        // zip with EROFS image, unaligned, so not mountable
        {
            TEST_DATA_DIR "/ralf-144mb-unaligned-image.zip",
            false,
            { 1539, 692224 },                                                            // data range
            { 1539 + 692224, 708608 - 692224 },                                          // hashes range
            fromHex("aa726db71f0fcbebf96a093562d3d980d5676273304ee91dda11c8bb9b8aa893"), // root hash
            fromHex("d0e3764079d184b2e90a7514805ee1aeb38ad038cc563412e1c37b793695e1d4")  // salt
        },
    };

    const auto bundle = createTestVerificationBundle();

    const std::filesystem::path someMountPoint = "/";
    const auto someFlags = MountFlag::ReadOnly | MountFlag::NoExec | MountFlag::NoSuid;

    for (const auto &test : packages)
    {
        int packageFd = open(test.path.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(packageFd, 0) << "failed to open " << test.path << " - " << strerror(errno);

        auto backingStore = OCIArchiveBackingStore::open(packageFd, true);
        ASSERT_FALSE(backingStore.isError())
            << "failed to open backing store for " << test.path << " due to " << backingStore.error().what();

        auto mockMounter = std::make_shared<StrictMock<MockDmVerityMounter>>();
        auto mockMountImpl = std::make_unique<MockPackageMountImpl>();

        OCIPackage package(std::move(backingStore.value()), mockMounter, nullptr);
        ASSERT_TRUE(package.isValid());

        if (test.expectMount)
        {
            EXPECT_TRUE(package.isMountable());

            // Matcher to check the imageFd passed to mount() points to the same file as packageFd (even though the fd
            // numbers will be different)
            const testing::Matcher<int> areSameFileMatcher = AreSameFile(packageFd);

            // Expect a call to mount, and return nullptr (we're not testing the mount here)
            EXPECT_CALL(*mockMounter, mount(_, IDmVerityMounter::FileSystemType::Erofs, areSameFileMatcher, someMountPoint,
                                            test.dataRange, test.hashesRange, test.rootHash, test.salt, someFlags))
                .WillOnce(Return(ByMove(std::move(mockMountImpl))));

            auto result = package.mount(someMountPoint, someFlags);
            EXPECT_FALSE(result.isError()) << "failed to mount " << test.path << " due to " << result.error().what();
        }
        else
        {
            EXPECT_FALSE(package.isMountable());

            // And the mount call should fail
            auto result = package.mount(someMountPoint, someFlags);
            EXPECT_TRUE(result.isError()) << "mount succeeded when should have failed for " << test.path;
        }

        ASSERT_EQ(close(packageFd), 0);
    }
}
