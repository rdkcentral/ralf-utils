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

#include "../utils/TempDirectory.h"
#include "Package.h"
#include "PackageReader.h"
#include "VerificationBundle.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

static VerificationBundle createTestVerificationBundle()
{
    auto cert = Certificate::loadFromFile(TEST_DATA_DIR "/certs/rootCA.cert.pem", Certificate::EncodingFormat::PEM);
    EXPECT_TRUE(cert) << "failed to load rootCA.cert.pem - " << cert.error().what();

    VerificationBundle bundle;
    bundle.addCertificate(cert.value());

    return bundle;
}

TEST(PackageExtractionTest, checkPackageSizeLimit)
{
    const std::list<std::filesystem::path> packages = {
        TEST_DATA_DIR "/widget-144mb.wgt",
        TEST_DATA_DIR "/ralf-144mb.tar",
    };

    VerificationBundle bundle = createTestVerificationBundle();

    for (const auto &packagePath : packages)
    {
        TempDirectory tempDir;
        ASSERT_TRUE(std::filesystem::exists(tempDir.path()));

        int testPackageFd = open(packagePath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << packagePath << " - " << strerror(errno);

        auto package = Package::open(testPackageFd, bundle, Package::OpenFlags::None);
        ASSERT_FALSE(package.isError()) << "failed to open " << packagePath << " due to " << package.error().what();

        package->setMaxExtractionBytes(128 * 1024 * 1024);

        Result<> result;

        // should fail as the package is over 14k files
        if (package->format() == Package::Format::Widget)
        {
            result = package->verify();
            EXPECT_FALSE(result) << "failed to enforce size limit on " << packagePath;
        }

        result = package->extractTo(tempDir.path());
        EXPECT_FALSE(result) << "failed to enforce size limit on " << packagePath;

        tempDir.removeContents();

        package->setMaxExtractionBytes(148 * 1024 * 1024);

        // should now succeed
        result = package->verify();
        EXPECT_TRUE(result) << "failed to enforce size limit on " << packagePath << " - " << result.error().what();

        result = package->extractTo(tempDir.path());
        EXPECT_TRUE(result) << "failed to remove size limit on " << packagePath << " - " << result.error().what();

        close(testPackageFd);
    }
}

TEST(PackageExtractionTest, checkPackageFileLimit)
{
    const std::list<std::filesystem::path> packages = {
        TEST_DATA_DIR "/widget-16k-files.wgt",
        TEST_DATA_DIR "/ralf-16k-files.tar",
    };

    VerificationBundle bundle = createTestVerificationBundle();

    for (const auto &packagePath : packages)
    {
        TempDirectory tempDir;
        ASSERT_TRUE(std::filesystem::exists(tempDir.path()));

        int testPackageFd = open(packagePath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << packagePath << " - " << strerror(errno);

        auto package = Package::open(testPackageFd, bundle, Package::OpenFlags::None);
        ASSERT_FALSE(package.isError()) << "failed to open " << packagePath << " due to " << package.error().what();

        package->setMaxExtractionEntries(14 * 1024);

        Result<> result;

        // should fail as the package is over 14k files
        if (package->format() == Package::Format::Widget)
        {
            result = package->verify();
            EXPECT_FALSE(result) << "failed to enforce entry limit on " << packagePath;
        }

        result = package->extractTo(tempDir.path());
        EXPECT_FALSE(result) << "failed to enforce entry limit on " << packagePath;

        tempDir.removeContents();

        package->setMaxExtractionEntries(18 * 1024);

        // should now succeed
        result = package->verify();
        EXPECT_TRUE(result) << "failed to enforce entry limit on " << packagePath << " - " << result.error().what();

        result = package->extractTo(tempDir.path());
        EXPECT_TRUE(result) << "failed to remove entry limit on " << packagePath << " - " << result.error().what();

        close(testPackageFd);
    }
}

TEST(PackageExtractionTest, emptyPackageExtractionCheck)
{
    const std::list<std::filesystem::path> packages = {
        TEST_DATA_DIR "/empty.ralf",
        TEST_DATA_DIR "/empty.erofs.ralf",
    };

    VerificationBundle bundle = createTestVerificationBundle();

    for (const auto &packagePath : packages)
    {
        TempDirectory tempDir;
        ASSERT_TRUE(std::filesystem::exists(tempDir.path()));

        int testPackageFd = open(packagePath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << packagePath << " - " << strerror(errno);

        auto package = Package::open(testPackageFd, bundle, Package::OpenFlags::None);
        ASSERT_FALSE(package.isError()) << "failed to open " << packagePath << " due to " << package.error().what();

        close(testPackageFd);

        // should succeed as the package is empty
        auto result = package->extractTo(tempDir.path());
        EXPECT_TRUE(result) << "failed to extract package " << packagePath << " - " << result.error().what();
        EXPECT_TRUE(std::filesystem::is_empty(tempDir.path())) << "extracted package " << packagePath << " is not empty";

        tempDir.removeContents();

        // and manually walk over the package contents to ensure no entries are present, but also no errors
        auto reader = PackageReader::create(package.value());
        EXPECT_FALSE(reader.hasError()) << "failed to create package reader for " << packagePath << " - "
                                        << reader.error().what();

        EXPECT_FALSE(reader.next().has_value())
            << "package " << packagePath << " contains entries when it should be empty";

        EXPECT_TRUE(!reader.hasError()) << "error occurred reading empty package " << packagePath << " - "
                                        << reader.error().what();
    }
}

static time_t getModificationTime(const std::filesystem::path &filePath)
{
    struct stat fileStat = {};
    if (stat(filePath.c_str(), &fileStat) != 0)
    {
        ADD_FAILURE() << "failed to stat file " << filePath << ": " << strerror(errno);
        return 0;
    }

    return fileStat.st_mtime;
}

TEST(PackageExtractionTest, modificationTimesPreserved)
{
    const std::list<std::filesystem::path> packages = {
        TEST_DATA_DIR "/modtimes.wgt",
        TEST_DATA_DIR "/modtimes.ralf",
        TEST_DATA_DIR "/modtimes.erofs.ralf",
    };

    const std::map<std::filesystem::path, time_t> expectedModTimes = {
        { "index.html", 1698828095 }, { "icon.png", 1759827763 },    { "rdk.config", 1720133391 },
        { "foo", 1768315167 },        { "foo/bar.txt", 1768315167 },
    };

    VerificationBundle bundle = createTestVerificationBundle();

    for (const auto &packagePath : packages)
    {
        TempDirectory tempDir;
        ASSERT_TRUE(std::filesystem::exists(tempDir.path()));

        int testPackageFd = open(packagePath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << packagePath << " - " << strerror(errno);

        auto package = Package::open(testPackageFd, bundle, Package::OpenFlags::None);
        ASSERT_FALSE(package.isError()) << "failed to open " << packagePath << " due to " << package.error().what();

        close(testPackageFd);

        auto result = package->extractTo(tempDir.path());
        EXPECT_TRUE(result) << "failed to extract package " << packagePath << " - " << result.error().what();

        // check modification times of extracted files and directories
        for (const auto &entry : expectedModTimes)
        {
            const std::filesystem::path entryPath = tempDir.path() / entry.first;
            ASSERT_TRUE(std::filesystem::exists(entryPath))
                << "expected extracted file " << entryPath << " does not exist";

            EXPECT_EQ(getModificationTime(entryPath), entry.second)
                << "modification time incorrect for " << entry.first << " in " << packagePath.filename();
        }

        tempDir.removeContents();
    }
}
