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

#include <filesystem>
#include <list>

#include <fcntl.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

static VerificationBundle createTestVerificationBundle()
{
    auto rsaCert = Certificate::loadFromFile(TEST_DATA_DIR "/certs/rootCA.cert.pem", Certificate::EncodingFormat::PEM);
    EXPECT_TRUE(rsaCert) << "failed to load rootCA.cert.pem - " << rsaCert.error().what();

    auto ecdsaCert =
        Certificate::loadFromFile(TEST_DATA_DIR "/certs/ecdsaRootCA.cert.pem", Certificate::EncodingFormat::PEM);
    EXPECT_TRUE(ecdsaCert) << "failed to load ecdsaRootCA.cert.pem - " << ecdsaCert.error().what();

    VerificationBundle bundle;
    bundle.addCertificate(rsaCert.value());
    bundle.addCertificate(ecdsaCert.value());

    return bundle;
}

TEST(PackageVerificationTest, basicVerificationCheck)
{
    // NOLINTBEGIN(bugprone-suspicious-missing-comma)
    const std::list<std::filesystem::path> packages = {
        TEST_DATA_DIR "/simple.wgt",
        TEST_DATA_DIR "/ralf-simple.tar",
        TEST_DATA_DIR "/ralf-simple.zip",
        TEST_DATA_DIR "/ralf-simple-ecdsa.zip",
        TEST_DATA_DIR "/widget-144mb.wgt",
        TEST_DATA_DIR "/widget-16k-files.wgt",
        TEST_DATA_DIR "/ralf-144mb.tar",
        TEST_DATA_DIR "/ralf-16k-files.tar",
        TEST_DATA_DIR "/ralf-144mb.zip",
        TEST_DATA_DIR "/ralf-144mb-compr-image.zip",
        TEST_DATA_DIR "/ralf-144mb-unaligned.tar",
        TEST_DATA_DIR "/ralf-144mb-unaligned-image.zip",
    };
    // NOLINTEND(bugprone-suspicious-missing-comma)

    VerificationBundle bundle = createTestVerificationBundle();

    for (const auto &packagePath : packages)
    {
        int testPackageFd = open(packagePath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << packagePath << " - " << strerror(errno);

        auto package = Package::open(testPackageFd, bundle, Package::OpenFlags::None);
        ASSERT_FALSE(package.isError()) << "failed to open " << packagePath << " due to " << package.error().what();

        auto result = package->verify();
        EXPECT_TRUE(result) << "failed to verify " << packagePath << " - " << result.error().what();

        close(testPackageFd);
    }
}

TEST(PackageVerificationTest, emptyPackageVerificationCheck)
{
    VerificationBundle bundle = createTestVerificationBundle();

    // NOLINTBEGIN(bugprone-suspicious-missing-comma)
    const std::list<std::filesystem::path> packages = {
        TEST_DATA_DIR "/empty.ralf",
        TEST_DATA_DIR "/empty.erofs.ralf",
    };
    // NOLINTEND(bugprone-suspicious-missing-comma)

    for (const auto &packagePath : packages)
    {
        int testPackageFd = open(packagePath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << packagePath << " - " << strerror(errno);

        auto package = Package::open(testPackageFd, bundle, Package::OpenFlags::None);
        ASSERT_FALSE(package.isError()) << "failed to open " << packagePath << " due to " << package.error().what();

        auto result = package->verify();
        EXPECT_TRUE(result) << "failed to verify " << packagePath << " - " << result.error().what();

        close(testPackageFd);
    }
}

TEST(PackageVerificationTest, expiredRalfSigningCertificateCheck)
{
    std::filesystem::path testDataDir = TEST_DATA_DIR;
    VerificationBundle bundle;

    auto nonExpiredRootCa = Certificate::loadFromFile(testDataDir / "expired-certs" / "root.crt");
    ASSERT_FALSE(nonExpiredRootCa.isError()) << "Failed to load non-expired root CA certificate";
    bundle.addCertificate(nonExpiredRootCa.value());

    auto expiredRootCa = Certificate::loadFromFile(testDataDir / "expired-certs" / "expired-root.crt");
    ASSERT_FALSE(expiredRootCa.isError()) << "Failed to load expired root CA certificate";
    bundle.addCertificate(expiredRootCa.value());

    // All these packages have some form of expired signing certificate (see expired-certs/README.md), so they
    // should fail verification if the CheckCertificateExpiry flag is set.
    const std::list<std::filesystem::path> expiredPackages = {
        testDataDir / "simple-expired-test1.ralf",
        testDataDir / "simple-expired-test2.ralf",
        testDataDir / "simple-expired-test3.ralf",
        testDataDir / "simple-expired-test4.ralf",
    };

    // Should verify OK as we are not checking for expired certificates
    for (const auto &expiredPackagePath : expiredPackages)
    {
        int testPackageFd = open(expiredPackagePath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << expiredPackagePath << " - " << strerror(errno);

        auto package = Package::open(testPackageFd, bundle, Package::OpenFlags::None);
        ASSERT_FALSE(package.isError()) << "failed to open " << expiredPackagePath << " due to "
                                        << package.error().what();

        auto result = package->verify();
        EXPECT_TRUE(result) << "failed to verify " << expiredPackagePath << " - " << result.error().what();

        close(testPackageFd);
    }

    // Should fail verification as we are checking for expired certificates
    for (const auto &expiredPackagePath : expiredPackages)
    {
        int testPackageFd = open(expiredPackagePath.c_str(), O_CLOEXEC | O_RDONLY);
        ASSERT_GE(testPackageFd, 0) << "failed to open " << expiredPackagePath << " - " << strerror(errno);

        auto package = Package::open(testPackageFd, bundle, Package::OpenFlags::CheckCertificateExpiry);
        ASSERT_TRUE(package.isError());
        EXPECT_STREQ(package.error().what(), "Invalid signing certificate chain");

        close(testPackageFd);
    }
}
