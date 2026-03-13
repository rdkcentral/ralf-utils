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

#include "Certificate.h"
#include "crypto/openssl/CertificateImpl.h"
#include "utils/FileUtils.h"
#include "utils/TimeUtils.h"

#include <gtest/gtest.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

static const char testCertPem[] = R"cert(-----BEGIN CERTIFICATE-----
MIIHejCCA2KgAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwRTELMAkGA1UEBhMCQVUx
EzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMg
UHR5IEx0ZDAgFw03MDAxMDEwMDAwMDBaGA8yMTE0MDgzMDExMTUzMVowPDELMAkG
A1UEBhMCVUsxEzARBgNVBAgMClNvbWUtU3RhdGUxCzAJBgNVBAoMAkFFMQswCQYD
VQQDDAJBRTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAL76nTA3P5ES
Oy4P/XfsTSpNvEIrc9fqeO0tvK8BN2VZQpwBXcYxqg4AVitq2LKTgMIEyHbiNF1w
vom3+IW9mmq8K5nfTTtADDkDkBmEY6FhbW/0P5wGaMW2moVAUFFiWp9NlzYdFrE2
PYk7IzTXaaZRVrBpK4RnmYsiavNlHz4gyFSu/jMvYE4iQy25caFCYkR6lTp8h7Yi
30iHPyzsTWdwdA8mrTzA3fbqNauBxtFtab64qWFmIbcsUoxw4rhJxskXE7KY/z8P
1uPJXngsckEAVcbjBq0/CDgp6vhaC90G1/fiCPtXj8rQlrbynTjmSs3NTmoypO78
sU26PfbO1YJ4/A/ISUST+anhWm+YQSsWBNMluzVzVXzTAYD0AwLXhCisn7pkgjql
osnGLdAv6NRJWWJ/Ae9GcRbHtImZzypucNj280C2hXIX8lHcHBdq9wc4QlOfs8lp
451z7EjiAsdSNO5WB9d90STyqHBulrJx/q3I6kEMdTbExsC59100iIrLS/58/D9D
y04cV/8+1E2fekB2s/60hBr+uITrGsCo+NZG8/CznRrKozt673JppmDAQYeIAdyP
/iIKwB3JHNe/LrbgRNWHpAOr+QggpQtrnnyQ4pPyp/t9WKODiWH13Ac8mqTXGruf
IPv41VtfVf/OF4l4Be8pgpw4yp3AGfLnAgMBAAGjezB5MAkGA1UdEwQCMAAwLAYJ
YIZIAYb4QgENBB8WHU9wZW5TU0wgR2VuZXJhdGVkIENlcnRpZmljYXRlMB0GA1Ud
DgQWBBRGzfeUxs5ShN6s31VrLtL1Gte7VTAfBgNVHSMEGDAWgBRZeC8r4PD/Wi0n
H1EQTttY1bRlZjANBgkqhkiG9w0BAQsFAAOCBAEADTSl4EB9DFKa4faVuMII4AgI
aIG/1Bl26CwxDV0m7m+ILyLJUd2OXp6ZHelInI76MWcnqENMlF4YzQNnQO4u8a1B
OhscYdhPJu7SRkbdMfD1a+83cmN9LHnxw0C+buKeeIDrnV9siwdu2hrVGcY0UdXT
r/9fP3JbQdO8oXxXe0QVoQRpppSkY+dQhYV2xT+XkoHBoWve3deuPEq9U7n8lDHA
Y2n6X5poV65qNxRNNxmIiYxifpH3Bu29oN+a9WFy51uDHk/GXRR8E8QAkMTq0TWN
AO36HpwV4fVXg+qMMohfPNSJTzxPqLJ5ZnZrRYMlD/1NGM+24PY5sjv3s7V7sr+q
iSS19vHmvPTLkSE1IzD6C47jF073cQZXpyIGbuaWs5d7Ykl8rgUaj8yw6+xMWayz
q8clR7P5SXdFtpLIbGw78TSPoUZYiyzKSIus8YhBsh0qFYdQGEwAZk/oWXpB6SV/
tl0Agc2hX5qR4qDibzw6n7CNiZH1npCiHn4KbWFhxXFs5dIIpWZD77daVN/W4MuV
/Fhirq5MGeWqo2CB9GBFKi3E9Od7AcE7QeHH2uBH0ZlcC7jEyWB/THkjIkxmN8nd
0Hhl+auEUHLYpk1vCBTILaLcfLx3Iclw3U57TvlNQYHL4Dq4/C/602hSxkl7IjOg
yls1CTeR1rhFhhks8LpCWTT2OTcZGBRkAxnoOkmwjFip7ZHF/HruvpGPwJgH3w1C
UVWyqBCxdH/Vhz/ZnYAp32y54kqq+RosEhGNSoO4uXeAqYC9aAVs0FaNhq4mgYKW
X0X0J86qXZT8ks/PNpl6N1g4V/V4UmTvC3oTQSRAC8ZC+LzDMxexZoTufOsPYyZ8
tn94a2mh/7GkocALFE/A2KEPnaXDQo0dC0HqDqx8WXMu6sPeq94MR3bzMN94N8Rk
+FrJZ0AHjEtKQW8p0fq+OSB+JbQTz8eWxDlCFME7oxue11K1Ysqe4z5npArDp2Cg
dsqWyQ01CrqbMjS91KcfoeIsMjgt0sp5aNu7C7lwuzKk4Qz17X93NllaM/ydEVmZ
PJB7IrSZNE9qMP/fF9BvX0WoqkFnTqyJsqIDvrir+k33Fl9I7eXZ8gGE5b9BAzwD
NV9FuXYoqV7pmTlaVZcviGv2vPsMD/sUrPF7t/6cTe0RQD1ygQj4o8qSdDzXRkBk
Q6JMlTbKRPc7QbzY3Ebz0xlxiJMLTl3aPBpGw4upQovoZuQCQw9ej1ZhQCk22Hr1
wGjvWVg4DmrKaP7I1rPR5Uth79pWhqUp/VbrMH/JUwwTMeNrlVkqD2k1+i/fTyuG
IPpkpGHREn2wBHsWzD/KV6uJUhJna+UEEK66XkMuIRrdXy9L53awhHhbrykANg==
-----END CERTIFICATE-----
)cert";

TEST(CertificateTest, CheckTestData)
{
    // test data files are stored using git LFS, therefore if the clone / checkout didn't have
    // LFS enabled, the test data files will be empty, this is a sanity check to ensure the test
    // data files are not empty ... it's not an actual unit test
    ASSERT_GE(std::filesystem::file_size(TEST_DATA_DIR "/test1.pem"), 1024);
}

TEST(CertificateTest, TestStandardOperators)
{
    Error err;

    // Ignore clang-tidy warning about use-after-move as this test explicitly tests the sanity of the move behaviour
    // NOLINTBEGIN(bugprone-use-after-move)

    auto result = Certificate::loadFromString(testCertPem);
    ASSERT_FALSE(result.isError()) << result.error().what();

    auto a = std::move(result.value());
    ASSERT_TRUE(a.isValid());
    ASSERT_FALSE(a.isNull());

    auto b = a;
    EXPECT_EQ(a, b);

    auto c = std::move(a);
    EXPECT_EQ(b, c);
    EXPECT_NE(b, a);

    EXPECT_FALSE(a.isValid());
    EXPECT_TRUE(a.isNull());
    EXPECT_EQ(a.commonName(), "");
    EXPECT_EQ(a.subject(), "");
    EXPECT_EQ(a.issuer(), "");

    // NOLINTEND(bugprone-use-after-move)
}

TEST(CertificateTest, CheckLoadPemCertificate)
{
    auto result = Certificate::loadFromString(testCertPem);
    ASSERT_FALSE(result.isError()) << result.error().what();

    auto a = std::move(result.value());
    ASSERT_TRUE(a.isValid());
    ASSERT_FALSE(a.isNull());

    EXPECT_EQ(a.commonName(), "AE");
    EXPECT_EQ(a.subject(), "CN=AE,O=AE,ST=Some-State,C=UK");
    EXPECT_EQ(a.issuer(), "O=Internet Widgits Pty Ltd,ST=Some-State,C=AU");
    EXPECT_EQ(a.notBefore(), toSystemTimePoint("1970-01-01T00:00:00Z"));
    EXPECT_EQ(a.notAfter(), toSystemTimePoint("2114-08-30T11:15:31Z"));

    result = Certificate::loadFromFile(TEST_DATA_DIR "/test1.pem", Certificate::PEM);
    ASSERT_FALSE(result.isError()) << result.error().what();

    auto b = std::move(result.value());
    ASSERT_TRUE(b.isValid());
    ASSERT_FALSE(b.isNull());

    EXPECT_EQ(a, b);
    EXPECT_EQ(a.commonName(), b.commonName());
    EXPECT_EQ(a.subject(), b.subject());
    EXPECT_EQ(a.issuer(), b.issuer());
    EXPECT_EQ(a.notBefore(), b.notBefore());
    EXPECT_EQ(a.notAfter(), b.notAfter());

    result = Certificate::loadFromFile(TEST_DATA_DIR "/test2.pem", Certificate::PEM);
    ASSERT_FALSE(result.isError()) << result.error().what();

    auto c = std::move(result.value());
    ASSERT_TRUE(c.isValid());
    ASSERT_FALSE(c.isNull());

    EXPECT_NE(a, c);
    EXPECT_NE(b, c);
    EXPECT_EQ(c.commonName(), "localhost");
}

TEST(CertificateTest, CheckLoadDerCertificate)
{
    auto result = Certificate::loadFromFile(TEST_DATA_DIR "/test1.der", Certificate::DER);
    ASSERT_FALSE(result.isError()) << result.error().what();

    auto a = std::move(result.value());
    ASSERT_TRUE(a.isValid());
    ASSERT_FALSE(a.isNull());

    EXPECT_EQ(a.commonName(), "AE");
    EXPECT_EQ(a.subject(), "CN=AE,O=AE,ST=Some-State,C=UK");
    EXPECT_EQ(a.issuer(), "O=Internet Widgits Pty Ltd,ST=Some-State,C=AU");

    EXPECT_EQ(a, Certificate::loadFromFile(TEST_DATA_DIR "/test1.pem", Certificate::PEM));

    result = Certificate::loadFromFile(TEST_DATA_DIR "/test2.der", Certificate::DER);
    ASSERT_FALSE(result.isError()) << result.error().what();

    auto b = std::move(result.value());
    ASSERT_TRUE(b.isValid());
    ASSERT_FALSE(b.isNull());
    EXPECT_EQ(b.notBefore(), toSystemTimePoint("2025-01-24T10:29:25Z"));
    EXPECT_EQ(b.notAfter(), toSystemTimePoint("2026-01-24T10:29:25Z"));

    EXPECT_EQ(b.commonName(), "localhost");

    EXPECT_EQ(b, Certificate::loadFromFile(TEST_DATA_DIR "/test2.pem", Certificate::PEM));

    EXPECT_NE(a, b);
}

TEST(CertificateTest, CheckParseFailures)
{
    auto a = Certificate::loadFromFile(TEST_DATA_DIR "/test1.der", Certificate::PEM);
    ASSERT_TRUE(a.isError());

    auto b = Certificate::loadFromVector(std::vector<uint8_t>{ testCertPem, testCertPem + sizeof(testCertPem) },
                                         Certificate::DER);
    ASSERT_TRUE(b.isError());
}

TEST(CertificateTest, CheckConvertToPemAndDer)
{
    Error err;

    auto a = CertificateImpl::loadFromFile(TEST_DATA_DIR "/test1.der", Certificate::DER, &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->toPem(), testCertPem);

    auto b = CertificateImpl::loadFromFile(TEST_DATA_DIR "/test1.pem", Certificate::PEM, &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->toDer(), fileContents(TEST_DATA_DIR "/test1.der"));
}

TEST(CertificateTest, LoadMultipleCerts)
{
    auto a = Certificate::loadFromFile(TEST_DATA_DIR "/test1.pem", Certificate::PEM);
    ASSERT_FALSE(a.isError()) << a.error().what();
    ASSERT_TRUE(a.value().isValid());

    auto b = Certificate::loadFromFile(TEST_DATA_DIR "/test2.pem", Certificate::PEM);
    ASSERT_FALSE(b.isError()) << b.error().what();
    ASSERT_TRUE(b.value().isValid());

    {
        auto certs = Certificate::loadFromFileMultiCerts(TEST_DATA_DIR "/concat-certs.pem");
        ASSERT_FALSE(certs.isError()) << certs.error().what();

        ASSERT_EQ(certs->size(), 2);
        auto it = certs->begin();
        EXPECT_EQ(*it, a);
        it = std::next(it);
        EXPECT_EQ(*it, b);
    }

    {
        auto certs = Certificate::loadFromFileMultiCerts(TEST_DATA_DIR "/concat-keys-certs.pem");
        ASSERT_FALSE(certs.isError()) << certs.error().what();

        ASSERT_EQ(certs->size(), 2);
        auto it = certs->begin();
        EXPECT_EQ(*it, a);
        it = std::next(it);
        EXPECT_EQ(*it, b);
    }
}

TEST(CertificateTest, LoadMultipleCertsFromMemory)
{
    auto a = Certificate::loadFromFile(TEST_DATA_DIR "/test1.pem", Certificate::PEM);
    ASSERT_FALSE(a.isError()) << a.error().what();
    ASSERT_TRUE(a->isValid());

    auto b = Certificate::loadFromFile(TEST_DATA_DIR "/test2.pem", Certificate::PEM);
    ASSERT_FALSE(b.isError()) << b.error().what();
    ASSERT_TRUE(b->isValid());

    {
        auto certs = Certificate::loadFromStringMultiCerts(fileStrContents(TEST_DATA_DIR "/concat-certs.pem"));
        ASSERT_FALSE(certs.isError()) << certs.error().what();
        ASSERT_EQ(certs->size(), 2);
        auto it = certs->begin();
        EXPECT_EQ(*it, a);
        it = std::next(it);
        EXPECT_EQ(*it, b);
    }

    {
        auto certs = Certificate::loadFromStringMultiCerts(fileStrContents(TEST_DATA_DIR "/concat-keys-certs.pem"));
        ASSERT_FALSE(certs.isError()) << certs.error().what();
        ASSERT_EQ(certs->size(), 2);
        auto it = certs->begin();
        EXPECT_EQ(*it, a);
        it = std::next(it);
        EXPECT_EQ(*it, b);
    }
}

TEST(CertificateTest, LoadExpiredCertificate)
{
    auto cert = Certificate::loadFromFile(TEST_DATA_DIR "/expired.crt", Certificate::PEM);
    ASSERT_FALSE(cert.isError()) << cert.error().what();
    ASSERT_TRUE(cert->isValid());

    EXPECT_EQ(cert->commonName(), "Expired Certificate");
    EXPECT_EQ(cert->subject(), "C=US,ST=Test State,O=Test Org,CN=Expired Certificate");
    EXPECT_EQ(cert->issuer(), "C=US,ST=Test State,O=Test Org,CN=Expired Certificate");

    // Check the certificate is expired
    EXPECT_TRUE(cert->notBefore() < std::chrono::system_clock::now());
    EXPECT_TRUE(cert->notAfter() < std::chrono::system_clock::now());
}

TEST(CertificateTest, LoadNotBeforeCertificate)
{
    auto cert = Certificate::loadFromFile(TEST_DATA_DIR "/future.crt", Certificate::PEM);
    ASSERT_FALSE(cert.isError()) << cert.error().what();
    ASSERT_TRUE(cert->isValid());

    EXPECT_EQ(cert->commonName(), "Future Certificate");
    EXPECT_EQ(cert->subject(), "C=US,ST=Test State,O=Test Org,CN=Future Certificate");
    EXPECT_EQ(cert->issuer(), "C=US,ST=Test State,O=Test Org,CN=Future Certificate");

    // Check the certificate is not yet valid
    EXPECT_TRUE(cert->notBefore() > std::chrono::system_clock::now());
    EXPECT_TRUE(cert->notAfter() > std::chrono::system_clock::now());
}
