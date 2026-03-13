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
#include "FileUtils.h"
#include "StringUtils.h"
#include "crypto/xmldsig/XmlDSigUtils.h"
#include "crypto/xmldsig/XmlDigitalSignature.h"

#include <gtest/gtest.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

namespace fs = std::filesystem;

TEST(XmlDSigSignatureTest, TestExpiredVerification)
{
    Error err;

    XmlDigitalSignature signature =
        XmlDigitalSignature::parse(fileStrContents(TEST_DATA_DIR "/xmldsig/expiredchain/test1/signature1.xml"), &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(signature.isNull());

    VerificationBundle bundle;
    EXPECT_FALSE(signature.verify(bundle, XmlDigitalSignature::VerifyOption::CheckCertificateExpiry, &err));

    auto rootCa = Certificate::loadFromFile(TEST_DATA_DIR "/xmldsig/expiredchain/root.crt", Certificate::PEM);
    ASSERT_FALSE(rootCa.isError()) << rootCa.error().what();
    ASSERT_FALSE(rootCa->isNull());

    bundle.addCertificate(rootCa.value());

    EXPECT_FALSE(signature.verify(bundle, XmlDigitalSignature::VerifyOption::CheckCertificateExpiry, &err));
    EXPECT_EQ(err.code(), ErrorCode::CertificateExpired);

    EXPECT_TRUE(signature.verify(bundle, XmlDigitalSignature::VerifyOption::None, &err));
    EXPECT_FALSE(err) << err.what();
}

TEST(XmlDSigSignatureTest, TestReadingCertificateChain)
{
    Error err;

    const fs::path dataDir = TEST_DATA_DIR;
    for (const fs::path &testDir : { fs::path("xmldsig/expiredchain/test1"), fs::path("xmldsig/expiredchain/test2"),
                                     fs::path("xmldsig/expiredchain/test3") })
    {
        XmlDigitalSignature signature =
            XmlDigitalSignature::parse(fileStrContents(dataDir / testDir / "signature1.xml"), &err);

        ASSERT_FALSE(err) << err.what();
        ASSERT_FALSE(signature.isNull());

        const auto certs = signature.signingCertificates(&err);
        ASSERT_FALSE(err) << err.what();
        ASSERT_EQ(certs.size(), 3);

        const auto intermediaCert1 =
            Certificate::loadFromFile(dataDir / testDir / "intermediate-1.crt", Certificate::PEM);
        ASSERT_FALSE(intermediaCert1.isError()) << intermediaCert1.error().what();
        ASSERT_FALSE(intermediaCert1->isNull());

        const auto intermediaCert2 =
            Certificate::loadFromFile(dataDir / testDir / "intermediate-2.crt", Certificate::PEM);
        ASSERT_FALSE(intermediaCert2.isError()) << intermediaCert2.error().what();
        ASSERT_FALSE(intermediaCert2->isNull());

        const auto signingCert = Certificate::loadFromFile(dataDir / testDir / "intermediate-3.crt", Certificate::PEM);
        ASSERT_FALSE(signingCert.isError()) << signingCert.error().what();
        ASSERT_FALSE(signingCert->isNull());

        const std::list<Certificate> expectedChain = { signingCert.value(), intermediaCert2.value(),
                                                       intermediaCert1.value() };
        EXPECT_EQ(certs, expectedChain);
    }
}

TEST(XmlDSigSignatureTest, TestReadingReferences)
{
    Error err;

    XmlDigitalSignature signature =
        XmlDigitalSignature::parse(fileStrContents(TEST_DATA_DIR "/xmldsig/expiredchain/test1/signature1.xml"), &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(signature.isNull());

    const auto references = signature.references(&err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(references.empty());

    ASSERT_EQ(references.size(), 4);

    EXPECT_EQ(references[0].uri, ".DS_Store");
    EXPECT_EQ(references[0].digestValue, fromBase64("1lFlJ5EFymdzGAUAaI30vcaaLHt3F1LwpG7xILf9jsM="));
    EXPECT_EQ(references[0].digestAlgorithm, XmlDigitalSignature::DigestAlgorithm::Sha256);

    EXPECT_EQ(references[1].uri, "config.xml");
    EXPECT_EQ(references[1].digestValue, fromBase64("YAjwI6pqlZ1wWmcGIBJwYv27hf2BhH2M0QQ46sm2i1Y="));
    EXPECT_EQ(references[1].digestAlgorithm, XmlDigitalSignature::DigestAlgorithm::Sha256);

    EXPECT_EQ(references[2].uri, "icon.png");
    EXPECT_EQ(references[2].digestValue, fromBase64("ZvPJ/GimznVs9e02gGkqgr6O9Yt44yCJrmffq/Cfj7s="));
    EXPECT_EQ(references[2].digestAlgorithm, XmlDigitalSignature::DigestAlgorithm::Sha256);

    EXPECT_EQ(references[3].uri, "start-godot.sh");
    EXPECT_EQ(references[3].digestValue, fromBase64("7IpC7cY6EnKM8konbD+LOnXe+062Oa0DG3/ABJM7xcE="));
    EXPECT_EQ(references[3].digestAlgorithm, XmlDigitalSignature::DigestAlgorithm::Sha256);
}

TEST(XmlDSigSignatureTest, TestReadingUriEncodedReferences)
{
    Error err;

    XmlDigitalSignature signature =
        XmlDigitalSignature::parse(fileStrContents(TEST_DATA_DIR "/xmldsig/large-signature.xml"), &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(signature.isNull());

    const auto references = signature.references(&err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(references.empty());
    ASSERT_EQ(references.size(), 4734);

    bool foundEncodedReference = false;

    for (const auto &ref : references)
    {
        EXPECT_FALSE(ref.uri.empty()) << "Reference URI is empty";
        EXPECT_FALSE(ref.digestValue.empty()) << "Reference digest value is empty";
        EXPECT_EQ(ref.digestAlgorithm, XmlDigitalSignature::DigestAlgorithm::Sha256)
            << "Unexpected digest algorithm for reference: " << ref.uri;

        if (ref.uri == "node_modules/es5-ext/array/#/@@iterator/implement.js")
        {
            foundEncodedReference = true;
            EXPECT_EQ(ref.digestValue, fromBase64("B1j4IVBT4h+0sb9h7CmkC/OYt0uDREW5rrYu1DmJyp4="));
        }
    }

    EXPECT_TRUE(foundEncodedReference) << "Encoded reference not found in the signature";
}

TEST(XmlDSigSignatureTest, TestReadingUriEncodedReferences2)
{
    Error err;

    XmlDigitalSignature signature =
        XmlDigitalSignature::parse(fileStrContents(TEST_DATA_DIR "/xmldsig/encref-signature.xml"), &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(signature.isNull());

    const auto references = signature.references(&err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(references.empty());

    ASSERT_EQ(references.size(), 4);

    EXPECT_EQ(references[0].uri, "Hello World");
    EXPECT_EQ(references[0].digestValue, fromBase64("1lFlJ5EFymdzGAUAaI30vcaaLHt3F1LwpG7xILf9jsM="));
    EXPECT_EQ(references[0].digestAlgorithm, XmlDigitalSignature::DigestAlgorithm::Sha256);

    EXPECT_EQ(references[1].uri, "!@#$%^&*");
    EXPECT_EQ(references[1].digestValue, fromBase64("YAjwI6pqlZ1wWmcGIBJwYv27hf2BhH2M0QQ46sm2i1Y="));
    EXPECT_EQ(references[1].digestAlgorithm, XmlDigitalSignature::DigestAlgorithm::Sha256);

    EXPECT_EQ(references[2].uri, "file://example.com/path?query=value");
    EXPECT_EQ(references[2].digestValue, fromBase64("ZvPJ/GimznVs9e02gGkqgr6O9Yt44yCJrmffq/Cfj7s="));
    EXPECT_EQ(references[2].digestAlgorithm, XmlDigitalSignature::DigestAlgorithm::Sha256);

    EXPECT_EQ(references[3].uri, "A B C");
    EXPECT_EQ(references[3].digestValue, fromBase64("7IpC7cY6EnKM8konbD+LOnXe+062Oa0DG3/ABJM7xcE="));
    EXPECT_EQ(references[3].digestAlgorithm, XmlDigitalSignature::DigestAlgorithm::Sha256);
}

TEST(XmlDSigSignatureTest, TestReadingEmptyUriReference)
{
    Error err;

    XmlDigitalSignature signature =
        XmlDigitalSignature::parse(fileStrContents(TEST_DATA_DIR "/xmldsig/emptyref-signature.xml"), &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(signature.isNull());

    const auto references = signature.references(&err);
    ASSERT_TRUE(err);
    ASSERT_TRUE(references.empty());
}

TEST(XmlDSigSignatureTest, TestReadingTooBigUriReference)
{
    Error err;

    XmlDigitalSignature signature =
        XmlDigitalSignature::parse(fileStrContents(TEST_DATA_DIR "/xmldsig/largeref-signature.xml"), &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(signature.isNull());

    const auto references = signature.references(&err);
    ASSERT_TRUE(err);
    ASSERT_TRUE(references.empty());
}

TEST(XmlDSigSignatureTest, TestReadingInvalidUriReference)
{
    Error err;

    XmlDigitalSignature signature =
        XmlDigitalSignature::parse(fileStrContents(TEST_DATA_DIR "/xmldsig/invalidref-signature.xml"), &err);
    ASSERT_TRUE(err);
    ASSERT_TRUE(signature.isNull());
}

TEST(XmlDSigSignatureTest, TestVerifyinhSignatureWithComments)
{
    Error err;

    XmlDigitalSignature signature =
        XmlDigitalSignature::parse(fileStrContents(TEST_DATA_DIR
                                                   "/xmldsig/expiredchain/test1/signature-with-comments.xml"),
                                   &err);
    ASSERT_FALSE(err) << err.what();
    ASSERT_FALSE(signature.isNull());

    auto rootCa = Certificate::loadFromFile(TEST_DATA_DIR "/xmldsig/expiredchain/root.crt", Certificate::PEM);
    ASSERT_FALSE(rootCa.isError()) << rootCa.error().what();
    ASSERT_FALSE(rootCa->isNull());

    VerificationBundle bundle;
    bundle.addCertificate(rootCa.value());
    EXPECT_TRUE(signature.verify(bundle, XmlDigitalSignature::VerifyOption::None, &err)) << err.what();
}
