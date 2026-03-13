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

#pragma once

#include "EnumFlags.h"
#include "Error.h"
#include "VerificationBundle.h"
#include "core/Compatibility.h"

#include <cinttypes>
#include <memory>
#include <string>
#include <vector>

class XmlDigitalSignatureImpl;

// -----------------------------------------------------------------------------
/*!
    \class XmlDigitalSignature
    \brief Parses and verifies an XML Digital Signature document.

    \note This object is not a complete implementation of the XML Digital Signature
    specification, it is a very strict subset that is used by the EntOS platform.
    In particular it only supports the SHA256 digest algorithm and X509 certificates.

    The object will parse and strictly schema validate the XML document, extracting
    key parts of the document.  You can then call verify() to verify the signature
    using a bundle of one or more root CA certificates.

    A Separate method is provided to extract the references and signing certificates
    from the document. These methods don't perform any verification steps beyond
    checking the document is well formed and contains the expected elements.  So
    typically you'd only call these methods after calling verify() to ensure the
    document is valid.

 */
class XmlDigitalSignature
{
public:
    static XmlDigitalSignature parse(const char *_Nonnull signatureXml, size_t signatureXmlLength,
                                     LIBRALF_NS::Error *_Nullable error = nullptr);

    static inline XmlDigitalSignature parse(const std::vector<uint8_t> &signatureXml,
                                            LIBRALF_NS::Error *_Nullable error = nullptr)
    {
        return parse(reinterpret_cast<const char *>(signatureXml.data()), signatureXml.size(), error);
    }

    static inline XmlDigitalSignature parse(const std::string &signatureXml, LIBRALF_NS::Error *_Nullable error = nullptr)
    {
        return parse(signatureXml.data(), signatureXml.size(), error);
    }

public:
    XmlDigitalSignature();
    XmlDigitalSignature(const XmlDigitalSignature &other);
    XmlDigitalSignature(XmlDigitalSignature &&other) noexcept;
    ~XmlDigitalSignature();

    XmlDigitalSignature &operator=(const XmlDigitalSignature &other);
    XmlDigitalSignature &operator=(XmlDigitalSignature &&other) noexcept;

    bool isNull() const;

    enum class VerifyOption : uint32_t
    {
        None = 0,
        CheckCertificateExpiry = (1 << 0),
    };

    LIBRALF_DECLARE_ENUM_FLAGS(VerifyOptions, VerifyOption)

    bool verify(const LIBRALF_NS::VerificationBundle &bundle, VerifyOptions options,
                LIBRALF_NS::Error *_Nullable error = nullptr) const;

    enum class DigestAlgorithm : uint32_t
    {
        Null = 0,

        // Sha1 = 1,
        Sha256 = 2,
        // Sha384 = 3,
        // Sha12 = 4
    };

    struct Reference
    {
        std::string uri;
        DigestAlgorithm digestAlgorithm = DigestAlgorithm::Null;
        std::vector<uint8_t> digestValue;
    };

    std::vector<Reference> references(LIBRALF_NS::Error *_Nullable error = nullptr) const;

    std::list<LIBRALF_NS::Certificate> signingCertificates(LIBRALF_NS::Error *_Nullable error = nullptr) const;

private:
    explicit XmlDigitalSignature(std::shared_ptr<XmlDigitalSignatureImpl> &&impl);
    std::shared_ptr<XmlDigitalSignatureImpl> m_impl;
};

LIBRALF_DECLARE_ENUM_FLAGS_OPERATORS(XmlDigitalSignature::VerifyOptions)
