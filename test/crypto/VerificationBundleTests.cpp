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

#include "FakeTime.h"
#include "FileUtils.h"
#include "VerificationBundle.h"
#include "crypto/openssl/CertificateImpl.h"
#include "crypto/openssl/VerificationBundleImpl.h"

#include <gtest/gtest.h>

#include <openssl/x509.h>

#include <algorithm>
#include <map>

#include <sys/time.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

TEST(VerificationBundleTest, TestAddCertificates)
{
    auto a = Certificate::loadFromFile(TEST_DATA_DIR "/test1.der", Certificate::DER);
    ASSERT_FALSE(a.isError()) << a.error().what();
    ASSERT_TRUE(a->isValid());

    auto b = Certificate::loadFromFile(TEST_DATA_DIR "/test2.pem", Certificate::PEM);
    ASSERT_FALSE(b.isError()) << b.error().what();
    ASSERT_TRUE(b->isValid());

    auto contains = [](const std::list<Certificate> &certs, const Certificate &cert) -> bool
    {
        for (const auto &c : certs)
        {
            if (c == cert)
                return true;
        }
        return false;
    };

    VerificationBundle bundle;
    EXPECT_TRUE(bundle.isEmpty());

    bundle.addCertificate(a.value());
    EXPECT_FALSE(bundle.isEmpty());
    EXPECT_TRUE(contains(bundle.certificates(), a.value()));
    EXPECT_FALSE(contains(bundle.certificates(), b.value()));

    bundle.addCertificate(b.value());
    EXPECT_TRUE(contains(bundle.certificates(), a.value()));
    EXPECT_TRUE(contains(bundle.certificates(), b.value()));

    bundle.clear();
    EXPECT_TRUE(bundle.isEmpty());
    EXPECT_EQ(bundle.certificates().size(), 0);

    bundle.addCertificates({ a.value(), b.value() });
    EXPECT_TRUE(contains(bundle.certificates(), a.value()));
    EXPECT_TRUE(contains(bundle.certificates(), b.value()));
}

TEST(VerificationBundleTest, TestCopyOperator)
{
    auto cert = Certificate::loadFromFile(TEST_DATA_DIR "/test1.der", Certificate::DER);
    ASSERT_FALSE(cert.isError()) << cert.error().what();
    ASSERT_TRUE(cert->isValid());
    ASSERT_FALSE(cert->isNull());

    VerificationBundle a;
    EXPECT_TRUE(a.isEmpty());

    a.addCertificate(cert.value());
    EXPECT_FALSE(a.isEmpty());

    VerificationBundle b = a;
    EXPECT_FALSE(a.isEmpty());
    EXPECT_FALSE(b.isEmpty());

    VerificationBundle c(a);
    EXPECT_FALSE(a.isEmpty());
    EXPECT_FALSE(b.isEmpty());
    EXPECT_FALSE(c.isEmpty());
}

TEST(VerificationBundleTest, TestMoveOperator)
{
    auto cert = Certificate::loadFromFile(TEST_DATA_DIR "/test1.der", Certificate::DER);
    ASSERT_FALSE(cert.isError()) << cert.error().what();
    ASSERT_TRUE(cert->isValid());
    ASSERT_FALSE(cert->isNull());

    // Ignore clang-tidy warning about use-after-move as this test explicitly tests the sanity of the move behaviour
    // NOLINTBEGIN(bugprone-use-after-move)

    auto a = VerificationBundle();
    EXPECT_TRUE(a.isEmpty());

    a.addCertificate(cert.value());
    EXPECT_FALSE(a.isEmpty());

    auto b = std::move(a);
    EXPECT_TRUE(a.isEmpty());
    EXPECT_FALSE(b.isEmpty());

    a = std::move(b);
    EXPECT_TRUE(b.isEmpty());
    EXPECT_FALSE(a.isEmpty());

    // NOLINTEND(bugprone-use-after-move)
}

TEST(VerificationBundleTest, TestFakeTimeWorks)
{
    FakeTime fakeTime("2032-08-30T11:15:31Z");

    const auto now = time(nullptr);
    EXPECT_EQ(now, 1977477331);

    std::tm *tm = std::gmtime(&now);
    EXPECT_EQ(tm->tm_year + 1900, 2032);
    EXPECT_EQ(tm->tm_mon + 1, 8);
    EXPECT_EQ(tm->tm_mday, 30);
    EXPECT_EQ(tm->tm_hour, 11);
    EXPECT_EQ(tm->tm_min, 15);
    EXPECT_EQ(tm->tm_sec, 31);

    // The following checks the OpenSSL has also had it's time adjusted
    ASN1_TIME *currentTime = ASN1_TIME_new();
    ASSERT_NE(currentTime, nullptr);
    ASSERT_NE(X509_gmtime_adj(currentTime, 0), nullptr);
    EXPECT_EQ(ASN1_TIME_cmp_time_t(currentTime, now), 0);

    std::tm asnTm = {};
    EXPECT_EQ(ASN1_TIME_to_tm(currentTime, &asnTm), 1);
    EXPECT_EQ(asnTm.tm_year + 1900, 2032);
    EXPECT_EQ(asnTm.tm_mon + 1, 8);
    EXPECT_EQ(asnTm.tm_mday, 30);
    EXPECT_EQ(asnTm.tm_hour, 11);
    EXPECT_EQ(asnTm.tm_min, 15);
    EXPECT_EQ(asnTm.tm_sec, 31);

    ASN1_TIME_free(currentTime);
}

TEST(VerificationBundleTest, TestPkcs7VerifyWithExpiredSigningCert)
{
    Error err;

    auto randomCert = CertificateImpl::loadFromFile(TEST_DATA_DIR "/test1.der", Certificate::DER, &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_NE(randomCert, nullptr);

    auto rootCert = CertificateImpl::loadFromFile(TEST_DATA_DIR "/expired/root.crt", Certificate::PEM, &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_NE(rootCert, nullptr);

    const std::list<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> blobs = {
        { fileContents(TEST_DATA_DIR "/expired/test1/root.hash"),
          fileContents(TEST_DATA_DIR "/expired/test1/root.hash.signed") },
        { fileContents(TEST_DATA_DIR "/expired/test2/root.hash"),
          fileContents(TEST_DATA_DIR "/expired/test2/root.hash.signed") },
        { fileContents(TEST_DATA_DIR "/expired/test3/root.hash"),
          fileContents(TEST_DATA_DIR "/expired/test3/root.hash.signed") },
    };

    std::vector<uint8_t> content;

    // should fail as the signing certificates have expired
    for (const auto &blob : blobs)
    {
        auto bundle = std::make_unique<VerificationBundleImpl>();
        bundle->addCertificate(randomCert);
        bundle->addCertificate(rootCert);

        EXPECT_FALSE(bundle->verifyPkcs7(blob.second.data(), blob.second.size(),
                                         VerificationBundle::VerifyOptions::CheckCertificateExpiry, &content, &err));
        EXPECT_TRUE(err);
        EXPECT_TRUE(content.empty());
    }

    // shift the time to match the certificates, should now pass
    {
        FakeTime fakeTime("2022-02-01T01:23:45Z");

        for (const auto &blob : blobs)
        {
            auto bundle = std::make_unique<VerificationBundleImpl>();
            bundle->addCertificate(randomCert);
            bundle->addCertificate(rootCert);

            EXPECT_TRUE(bundle->verifyPkcs7(blob.second.data(), blob.second.size(),
                                            VerificationBundle::VerifyOptions::CheckCertificateExpiry, &content, &err));
            EXPECT_FALSE(err) << err.what();
            EXPECT_EQ(content, blob.first);
        }
    }

    // shift the time way into the future, should fail again
    {
        FakeTime fakeTime("2060-02-01T01:23:45Z");

        for (const auto &blob : blobs)
        {
            auto bundle = std::make_unique<VerificationBundleImpl>();
            bundle->addCertificate(randomCert);
            bundle->addCertificate(rootCert);

            EXPECT_FALSE(bundle->verifyPkcs7(blob.second.data(), blob.second.size(),
                                             VerificationBundle::VerifyOptions::CheckCertificateExpiry, &content, &err));
            EXPECT_TRUE(err);
            EXPECT_TRUE(content.empty());
        }
    }

    // shift the time way into the past, should fail again
    {
        FakeTime fakeTime("2000-02-01T01:23:45Z");

        for (const auto &blob : blobs)
        {
            auto bundle = std::make_unique<VerificationBundleImpl>();
            bundle->addCertificate(randomCert);
            bundle->addCertificate(rootCert);

            EXPECT_FALSE(bundle->verifyPkcs7(blob.second.data(), blob.second.size(),
                                             VerificationBundle::VerifyOptions::CheckCertificateExpiry, &content, &err));
            EXPECT_TRUE(err);
            EXPECT_TRUE(content.empty());
        }
    }

    // go back to current time and disable certificate checking
    {
        for (const auto &blob : blobs)
        {
            auto bundle = std::make_unique<VerificationBundleImpl>();
            bundle->addCertificate(randomCert);
            bundle->addCertificate(rootCert);

            EXPECT_TRUE(bundle->verifyPkcs7(blob.second.data(), blob.second.size(),
                                            VerificationBundle::VerifyOptions::None, &content, &err));
            EXPECT_FALSE(err) << err.what();
            EXPECT_EQ(content, blob.first);
        }
    }

    // and should work in the valid range as well
    {
        FakeTime fakeTime("2022-02-01T01:23:45Z");

        for (const auto &blob : blobs)
        {
            auto bundle = std::make_unique<VerificationBundleImpl>();
            bundle->addCertificate(randomCert);
            bundle->addCertificate(rootCert);

            EXPECT_TRUE(bundle->verifyPkcs7(blob.second.data(), blob.second.size(),
                                            VerificationBundle::VerifyOptions::None, &content, &err));
            EXPECT_FALSE(err) << err.what();
            EXPECT_EQ(content, blob.first);
        }
    }
}

TEST(VerificationBundleTest, TestPkcs7VerifyWithExpiredRootCA)
{
    Error err;

    auto rootCert =
        CertificateImpl::loadFromFile(TEST_DATA_DIR "/expired/test4/expired-root.crt", Certificate::PEM, &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_NE(rootCert, nullptr);

    auto randomCert = CertificateImpl::loadFromFile(TEST_DATA_DIR "/test1.der", Certificate::DER, &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_NE(randomCert, nullptr);

    auto bundle = std::make_unique<VerificationBundleImpl>();
    bundle->addCertificate(rootCert);
    bundle->addCertificate(randomCert);

    const auto blob = fileContents(TEST_DATA_DIR "/expired/test4/root.hash.signed");
    ASSERT_FALSE(blob.empty());

    std::vector<uint8_t> content;

    // should fail as the CA certificate has expired
    EXPECT_FALSE(bundle->verifyPkcs7(blob.data(), blob.size(),
                                     VerificationBundle::VerifyOptions::CheckCertificateExpiry, &content, &err));
    EXPECT_TRUE(err);
    EXPECT_TRUE(content.empty());

    // set the ignore certificate time and check it works
    EXPECT_TRUE(bundle->verifyPkcs7(blob.data(), blob.size(), VerificationBundle::VerifyOptions::None, &content, &err));
    EXPECT_FALSE(err) << err.what();
    EXPECT_EQ(content, fileContents(TEST_DATA_DIR "/expired/test4/root.hash"));
}

TEST(VerificationBundleTest, TestCertificateVerification)
{
    auto rootCert = Certificate::loadFromFile(TEST_DATA_DIR "/xmldsig/expiredchain/root.crt", Certificate::PEM);
    ASSERT_FALSE(rootCert.isError()) << rootCert.error().what();
    ASSERT_TRUE(rootCert->isValid());

    auto randomCert = Certificate::loadFromFile(TEST_DATA_DIR "/test1.der", Certificate::DER);
    ASSERT_FALSE(randomCert.isError()) << randomCert.error().what();
    ASSERT_TRUE(randomCert->isValid());

    VerificationBundle bundle;
    bundle.addCertificate(rootCert.value());
    bundle.addCertificate(randomCert.value());

    auto intermediaCert1 =
        Certificate::loadFromFile(TEST_DATA_DIR "/xmldsig/expiredchain/test1/intermediate-1.crt", Certificate::PEM);
    ASSERT_FALSE(intermediaCert1.isError());
    auto intermediaCert2 =
        Certificate::loadFromFile(TEST_DATA_DIR "/xmldsig/expiredchain/test1/intermediate-2.crt", Certificate::PEM);
    ASSERT_FALSE(intermediaCert2.isError());
    auto intermediaCert3 =
        Certificate::loadFromFile(TEST_DATA_DIR "/xmldsig/expiredchain/test1/intermediate-3.crt", Certificate::PEM);
    ASSERT_FALSE(intermediaCert3.isError());

    auto result = bundle.verifyCertificate(intermediaCert1.value(), VerificationBundle::VerifyOptions::None);
    EXPECT_TRUE(result);
    EXPECT_FALSE(result.isError()) << result.error().what();

    std::list<Certificate> certs;
    certs.push_back(intermediaCert1.value());

    result = bundle.verifyCertificate(intermediaCert2.value(), certs, VerificationBundle::VerifyOptions::None);
    EXPECT_TRUE(result) << result.error().what();

    certs.push_back(intermediaCert2.value());

    result = bundle.verifyCertificate(intermediaCert3.value(), certs, VerificationBundle::VerifyOptions::None);
    EXPECT_TRUE(result) << result.error().what();

    std::reverse(certs.begin(), certs.end());

    result = bundle.verifyCertificate(intermediaCert3.value(), certs, VerificationBundle::VerifyOptions::None);
    EXPECT_TRUE(result) << result.error().what();
}
