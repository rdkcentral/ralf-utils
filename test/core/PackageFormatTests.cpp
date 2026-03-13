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

#include "Package.h"
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

TEST(PackageFormatTest, checkDetectionCode)
{
    struct TestPackage
    {
        std::filesystem::path path;
        Package::Format format;
    };

    const std::vector<TestPackage> packages = {
        { TEST_DATA_DIR "/ralf-16k-files.tar", Package::Format::Ralf },
        { TEST_DATA_DIR "/ralf-144mb.tar", Package::Format::Ralf },
        { TEST_DATA_DIR "/simple.wgt", Package::Format::Widget },
        { TEST_DATA_DIR "/widget-16k-files.wgt", Package::Format::Widget },
        { TEST_DATA_DIR "/widget-144mb.wgt", Package::Format::Widget },
        { TEST_DATA_DIR "/simple.erofs.wgt", Package::Format::Unknown },
    };

    for (const auto &testPackage : packages)
    {
        int testPackageFd = open(testPackage.path.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << testPackage.path << " - " << strerror(errno);

        EXPECT_EQ(Package::detectPackageFormat(testPackageFd), testPackage.format);

        close(testPackageFd);
    }
}

TEST(PackageFormatTest, incorrectFormatOpenFails)
{
    struct TestPackage
    {
        std::filesystem::path path;
        Package::Format format;
    };

    const std::vector<TestPackage> packages = {
        { TEST_DATA_DIR "/ralf-16k-files.tar", Package::Format::Ralf },
        { TEST_DATA_DIR "/ralf-144mb.tar", Package::Format::Ralf },
        { TEST_DATA_DIR "/simple.wgt", Package::Format::Widget },
        { TEST_DATA_DIR "/widget-16k-files.wgt", Package::Format::Widget },
        { TEST_DATA_DIR "/widget-144mb.wgt", Package::Format::Widget },
    };

    const std::vector<Package::Format> formats = { Package::Format::Unknown, Package::Format::Widget,
                                                   Package::Format::Ralf };

    const auto bundle = createTestVerificationBundle();

    for (const auto &testPackage : packages)
    {
        int testPackageFd = open(testPackage.path.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << testPackage.path << " - " << strerror(errno);

        for (const auto format : formats)
        {
            auto package = Package::open(testPackageFd, bundle, Package::OpenFlag::None, format);
            if (format == testPackage.format)
            {
                EXPECT_FALSE(package.isError())
                    << "Failed to open package " << testPackage.path << " using expected format " << format;
            }
            else
            {
                EXPECT_TRUE(package.isError())
                    << "Succeeded to open package " << testPackage.path << " using wrong format " << format;
            }
        }

        close(testPackageFd);
    }
}
