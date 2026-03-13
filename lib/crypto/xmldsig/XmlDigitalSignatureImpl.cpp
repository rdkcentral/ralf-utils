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

#include "XmlDigitalSignatureImpl.h"
#include "XmlDSigSchema.h"
#include "XmlDSigUtils.h"
#include "core/DigitalSignature.h"
#include "core/LogMacros.h"
#include "core/Utils.h"
#include "core/XmlSchemaValidator.h"

#include <libxml/c14n.h>
#include <libxml/uri.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

// -----------------------------------------------------------------------------
/*!
    \internal

    Creates the actual implementation of the XmlDigitalSignature class.  This
    schema checks the document and extracts the key parts of the signature.

    If it all checks out then a new XmlDigitalSignatureImpl object is created
    and returned inside a shared pointer.  If there is an error then the \a error
    object is populated and nullptr is returned.

 */
std::shared_ptr<XmlDigitalSignatureImpl> XmlDigitalSignatureImpl::createImpl(xmlDoc *document, Error *_Nullable error)
{
    if (error)
        error->clear();

    if (!document)
        return nullptr;

    // Schema validate the document
    XmlSchemaValidator validator(XMLDSIG_SCHEMA, compileTimeStrLen(XMLDSIG_SCHEMA));
    if (!validator.isValid() || !validator.validate(document, error))
    {
        // Error already set by the schema validator
        return nullptr;
    }

    // Get the root node for extracting key parts of the xml document
    xmlNodePtr root = xmlDocGetRootElement(document);
    if (!root || xmlStrcmp(root->name, BAD_CAST "Signature") != 0)
    {
        logWarning("Invalid signature document, invalid root element");
        return nullptr;
    }

    xmlNode *signedInfoNode = nullptr;
    xmlNode *signedValueNode = nullptr;
    xmlNode *keyInfoNode = nullptr;

    // Get the 3 key sections of the document, the SignedInfo, SignatureValue and KeyInfo elements.
    for (xmlNodePtr node = root->children; node; node = node->next)
    {
        if (node->type != XML_ELEMENT_NODE)
            continue;

        if (!signedInfoNode && (xmlStrcmp(node->name, BAD_CAST "SignedInfo") == 0))
            signedInfoNode = node;
        else if (!signedValueNode && (xmlStrcmp(node->name, BAD_CAST "SignatureValue") == 0))
            signedValueNode = node;
        else if (!keyInfoNode && (xmlStrcmp(node->name, BAD_CAST "KeyInfo") == 0))
            keyInfoNode = node;
    }

    // If the document is missing any of the key sections then it's invalid, this shouldn't happen as the schema
    // check should have already caught this.
    if (!signedInfoNode || !signedValueNode || !keyInfoNode)
    {
        logWarning("Invalid signature document, missing key sections");
        return nullptr;
    }

    // So far so good so wrap in an implementation
    return std::make_shared<XmlDigitalSignatureImpl>(document, signedInfoNode, signedValueNode, keyInfoNode);
}

XmlDigitalSignatureImpl::XmlDigitalSignatureImpl(xmlDocPtr document, xmlNodePtr signedInfoNode,
                                                 xmlNodePtr signedValueNode, xmlNodePtr keyInfoNode)
    : m_document(document)
    , m_signedInfoNode(signedInfoNode)
    , m_signedValueNode(signedValueNode)
    , m_keyInfoNode(keyInfoNode)
{
}

XmlDigitalSignatureImpl::~XmlDigitalSignatureImpl()
{
    if (m_document)
        xmlFreeDoc(m_document);
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Returns 1 if the node should be included in the canonicalization, this means
    that we check the supplied node is the SignedInfo node or a child of it.

    Comment nodes are excluded from the canonicalization, as per the
    http://www.w3.org/2006/12/xml-c14n11 specification.

 */
int XmlDigitalSignatureImpl::isSignedInfoNode(void *_Nonnull userData, xmlNodePtr _Nullable node,
                                              xmlNodePtr _Nullable parent)
{
    auto *self = static_cast<XmlDigitalSignatureImpl *>(userData);

    // Note that our schema validation requires that the CanonicalizationMethod is http://www.w3.org/2006/12/xml-c14n11
    // not the http://www.w3.org/2006/12/xml-c14n11#WithComments so we should not include comments in the
    // canonicalization process.
    if (!node || (node->type == XML_COMMENT_NODE))
        return 0;

    // If the node is the SignedInfo node then return 1
    if (node == self->m_signedInfoNode)
        return 1;

    // Otherwise walk back up the parent chain to see if the node is a child of the SignedInfo node
    while (parent != nullptr)
    {
        if (parent == self->m_signedInfoNode)
            return 1;

        parent = parent->parent;
    }

    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Returns the canonicalized SignedInfo node from the signature document. If an
    error occurs then an empty string is returned and \a error is populated if
    it is not null.

 */
std::string XmlDigitalSignatureImpl::canonicalizedSignedInfo(Error *_Nullable error) const
{
    if (error)
        error->clear();

    // Create a buffer to store the canonicalized output
    auto buf = xmlOutputBufferUniquePtr(xmlAllocOutputBuffer(nullptr), xmlOutputBufferClose);
    if (!buf)
    {
        if (error)
            error->assign(ErrorCode::InternalError, "Failed to allocate xml output buffer");
        return {};
    }

    // Canonicalize the SignedInfo node and children into the buffer
    int ret = xmlC14NExecute(m_document, &XmlDigitalSignatureImpl::isSignedInfoNode,
                             const_cast<XmlDigitalSignatureImpl *>(this), XML_C14N_1_1, nullptr, 0, buf.get());
    if (ret < 0)
    {
        if (error)
            error->assign(ErrorCode::GenericXmlError, "Failed to canonicalize the SignedInfo node");
        return {};
    }

    size_t len = xmlBufUse(buf->buffer);
    if (len == 0)
    {
        return {};
    }

    return { reinterpret_cast<const char *>(xmlBufContent(buf->buffer)), len };
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Returns the base64 decoded signature value from the signature document.

 */
std::vector<uint8_t> XmlDigitalSignatureImpl::signatureValue(Error *_Nullable error) const
{
    if (error)
        error->clear();

    // Get the base64 encoded signature value
    auto value = xmlCharUniquePtr(xmlNodeGetContent(m_signedValueNode), xmlFree);
    if (!value)
    {
        if (error)
            error->assign(ErrorCode::GenericXmlError, "Failed to get the signature value");
        return {};
    }

    // Convert the base64 encoded signature value to a byte array
    return base64Decode(value.get(), xmlStrlen(value.get()), error);
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    \warning The \a unsortedCerts is modified by this function.

    Attempts to sort the certificates in \a signingCerts into the correct order.
    The _correct order_ is that the signing certificate is first, followed by any
    intermediate certificates in the chain.  So each certificate in the chain
    certifies the one preceding it.

    The xmldsig specification specifies that the X509 objects can be in any order,
    so we need to do some checks and sort them.  However the current official
    signer uses xmlsec1 which does put the certificates in the same order in
    the xml document.

 */
std::list<Certificate> XmlDigitalSignatureImpl::sortCertificates(std::vector<Certificate> *_Nonnull unsortedCerts,
                                                                 Error *_Nullable error)
{
    if (error)
        error->clear();

    if (unsortedCerts->size() < 2)
    {
        // Nothing to do
        return { unsortedCerts->front() };
    }

    // Find the signing leaf certificate (the one that is not the issuer of any other certificates in the chain)
    std::list<Certificate> sorted;
    for (auto &cert : *unsortedCerts)
    {
        logDebug("Checking if have issuer for cert: issuer:'%s' subject:'%s'", cert.issuer().c_str(),
                 cert.subject().c_str());

        bool isIssuer = false;
        for (const auto &other : *unsortedCerts)
        {
            if (!cert.isNull() && !other.isNull() && (cert.subject() == other.issuer()))
            {
                isIssuer = true;
                break;
            }
        }

        if (!isIssuer)
        {
            logDebug("Found signing cert: issuer:'%s' subject:'%s'", cert.issuer().c_str(), cert.subject().c_str());
            sorted.emplace_back(std::move(cert));
            break;
        }
    }

    // There should only be one signer leaf certificate, if not then something is wrong in the signature
    if (sorted.size() != 1)
    {
        if (error)
            error->assign(ErrorCode::GenericCryptoError, "Failed to find the signing certificate in the chain");

        return {};
    }

    // Now add the intermediate certificates in the correct order
    while (true)
    {
        bool added = false;
        for (auto &cert : *unsortedCerts)
        {
            if (!cert.isNull() && (cert.subject() == sorted.back().issuer()))
            {
                logDebug("Found next cert: issuer:'%s' subject:'%s'", cert.issuer().c_str(), cert.subject().c_str());
                sorted.emplace_back(std::move(cert));
                added = true;
                break;
            }
        }

        if (!added)
            break;
    }

    // If we didn't add all the certificates then something is wrong in the signature
    if (sorted.size() != unsortedCerts->size())
    {
        if (error)
            error->assign(ErrorCode::GenericCryptoError, "Failed to sort the certificate chain");

        return {};
    }

    return sorted;
}

// -----------------------------------------------------------------------------
/*!
    Extracts the certificates from the KeyInfo section of the signature document.
    It returns the base64 decoded certificates as a list of byte arrays.

 */
std::list<Certificate> XmlDigitalSignatureImpl::signingCertificates(Error *_Nullable error) const
{
    if (error)
        error->clear();

    std::vector<Certificate> unsortedCerts;
    unsortedCerts.reserve(8);

    // Iterates through the X509Data/X509Certificate nodes in the KeyInfo section
    for (xmlNodePtr node = m_keyInfoNode->children; node; node = node->next)
    {
        if ((node->type == XML_ELEMENT_NODE) && xmlStrEqual(node->name, BAD_CAST "X509Data"))
        {
            for (xmlNodePtr certNode = node->children; certNode; certNode = certNode->next)
            {
                if ((certNode->type == XML_ELEMENT_NODE) && xmlStrEqual(certNode->name, BAD_CAST "X509Certificate"))
                {
                    auto certValue = xmlCharUniquePtr(xmlNodeGetContent(certNode), xmlFree);
                    if (certValue)
                    {
                        // Try and convert the certificate to a byte array
                        const auto certData = base64Decode(certValue.get(), xmlStrlen(certValue.get()), error);
                        if (certData.empty())
                        {
                            // Error already captured
                            return {};
                        }

                        // Parse the certificate data into an X509 structure
                        auto result = Certificate::loadFromVector(certData, Certificate::EncodingFormat::DER);
                        if (!result)
                        {
                            if (error)
                                *error = result.error();

                            return {};
                        }

                        // Add the certificate to the back of the list
                        unsortedCerts.emplace_back(std::move(result.value()));
                    }
                }
            }
        }
    }

    // If we failed to read any certificates then this is an error.  The schema validation should have
    // already checked this.  Nb: the xmldsig standard allows no certificate chain, but we don't.
    if (unsortedCerts.empty())
    {
        if (error)
            error->assign(ErrorCode::GenericXmlError, "No certificates found in the KeyInfo section");
        return {};
    }

    // Sort the certificates into the correct order, with the leaf node at the beginning followed by intermediate
    // certificates.
    return sortCertificates(&unsortedCerts, error);
}

// -----------------------------------------------------------------------------
/*!
    Verifies the signature of the document using the certificates in the KeyInfo
    section of the document.

    The process for verification is as follows:
        1. Get the canonicalized SignedInfo node from the document.
        2. Get the signing certificate and intermediate certificates from the KeyInfo section.
        3. Verify the signing certificate and intermediate certificates against the root CA in the \a bundle.
        4. Use the public key from the signing certificate to verify the signature value.

 */
bool XmlDigitalSignatureImpl::verify(const VerificationBundle &bundle, XmlDigitalSignature::VerifyOptions options,
                                     Error *_Nullable error) const
{
    if (error)
        error->clear();

    // Get the canonicalized signed info xml block - this is what is signed
    const std::string c14nSignedInfo = canonicalizedSignedInfo(error);
    if (c14nSignedInfo.empty())
    {
        if (error && !error->operator bool())
            error->assign(ErrorCode::XmlParseError, "Empty canonicalized <SignedInfo> element");
        return false;
    }

    // Get the signature value, the value to compare for verification
    const std::vector<uint8_t> sigValue = signatureValue(error);
    if (sigValue.empty())
    {
        if (error && !error->operator bool())
            error->assign(ErrorCode::XmlParseError, "Empty <SignatureValue> element");
        return false;
    }

    // Get the sorted list of signing certificate and intermediate certificates
    std::list<Certificate> signingCerts = signingCertificates(error);
    if (signingCerts.empty())
    {
        if (error && !error->operator bool())
            error->assign(ErrorCode::XmlParseError, "Missing <X509Certificate> element(s)");
        return false;
    }

    Certificate signingCert = std::move(signingCerts.front());
    signingCerts.pop_front();

    // Convert the verification options
    VerificationBundle::VerifyOptions verifyOptions = VerificationBundle::VerifyOption::None;
    if ((options & XmlDigitalSignature::VerifyOption::CheckCertificateExpiry) ==
        XmlDigitalSignature::VerifyOption::CheckCertificateExpiry)
        verifyOptions |= VerificationBundle::VerifyOption::CheckCertificateExpiry;

    // Use the verification bundle to verify the certificate chain from the signing certificate down to the root CA
    const auto result = bundle.verifyCertificate(signingCert, signingCerts, verifyOptions);
    if (!result)
    {
        if (error)
            *error = result.error();

        return false;
    }

    // Now we can verify the signature, get the public key from the signing certificate
    DigitalSignature sigVerifier(DigitalSignature::Algorithm::Sha256);

    // Add the data we're verifying to the signature across
    sigVerifier.update(c14nSignedInfo);

    // Finally verify the signature of the data using the verified public key from the signing cert
    return sigVerifier.verify(signingCert, sigValue, error);
}

// -----------------------------------------------------------------------------
/*!
    \warning This does NOT perform any verification steps besides verify that
    the document is well formed and contains the expected elements.  You should
    call verify() to perform the actual verification steps beforehand.


 */
std::vector<XmlDigitalSignature::Reference> XmlDigitalSignatureImpl::references(Error *_Nullable error) const
{
    if (error)
        error->clear();

    // The number of child elements in the SignedInfo node is a reasonable estimate of the number of references
    // so we reserve space for that many references.
    unsigned long n = xmlChildElementCount(m_signedInfoNode);

    std::vector<XmlDigitalSignature::Reference> refs;
    refs.reserve(n);

    //
    char uriDecoded[PATH_MAX];

    // Iterates through the Reference nodes in the SignedInfo section
    for (xmlNodePtr node = m_signedInfoNode->children; node; node = node->next)
    {
        if ((node->type == XML_ELEMENT_NODE) && xmlStrEqual(node->name, BAD_CAST "Reference"))
        {
            auto uri = xmlCharUniquePtr(xmlGetProp(node, BAD_CAST "URI"), xmlFree);
            if (!uri)
            {
                if (error)
                    error->assign(ErrorCode::XmlParseError, "Failed to get the 'URI' for a reference node");
                return {};
            }

            auto uriLen = xmlStrlen(uri.get());
            if ((uriLen == 0) || (uriLen >= (PATH_MAX - 1)))
            {
                if (error)
                    error->assign(ErrorCode::XmlParseError,
                                  "Empty or invalid 'URI' for a reference node in the xml signature");
                return {};
            }

            uriDecoded[0] = '\0';
            xmlURIUnescapeString(reinterpret_cast<const char *>(uri.get()), uriLen, uriDecoded);

            XmlDigitalSignature::Reference reference;
            reference.uri = uriDecoded;
            reference.digestAlgorithm = XmlDigitalSignature::DigestAlgorithm::Null;

            for (xmlNodePtr child = node->children; child; child = child->next)
            {
                if (child->type != XML_ELEMENT_NODE)
                    continue;

                if (xmlStrEqual(child->name, BAD_CAST "DigestMethod"))
                {
                    // The algorithm should have already been checked by the schema validator, but just in
                    // case we do it here again
                    auto algorithm = xmlCharUniquePtr(xmlGetProp(child, BAD_CAST "Algorithm"), xmlFree);
                    if (!algorithm)
                    {
                        if (error)
                            error->assign(ErrorCode::XmlParseError,
                                          "Failed to get the 'Algorithm' for a reference node");
                        return {};
                    }

                    // Sanity check the algorithm
                    if (xmlStrEqual(algorithm.get(), BAD_CAST "http://www.w3.org/2001/04/xmlenc#sha256"))
                    {
                        reference.digestAlgorithm = XmlDigitalSignature::DigestAlgorithm::Sha256;
                    }
                    else
                    {
                        if (error)
                            error->assign(ErrorCode::XmlParseError, "Invalid digest algorithm for a reference node");
                        return {};
                    }
                }
                else if (xmlStrEqual(child->name, BAD_CAST "DigestValue"))
                {
                    auto digestBase64 = xmlCharUniquePtr(xmlNodeGetContent(child), xmlFree);
                    if (!digestBase64)
                    {
                        if (error)
                            error->assign(ErrorCode::XmlParseError,
                                          "Failed to get the 'DigestValue' for a reference node");
                        return {};
                    }

                    reference.digestValue = base64Decode(digestBase64.get(), xmlStrlen(digestBase64.get()), error);
                    if (reference.digestValue.empty())
                    {
                        logWarning("Failed to base64 decode the digest value for reference with uri '%s' in the xml "
                                   "signature",
                                   reference.uri.c_str());

                        // Error object already set
                        return {};
                    }
                }
            }

            // Processed a <Reference> node so check we managed to get all the required fields
            if (reference.uri.empty() || reference.digestAlgorithm == XmlDigitalSignature::DigestAlgorithm::Null ||
                reference.digestValue.empty())
            {
                if (error)
                    error->assign(ErrorCode::XmlParseError,
                                  "Failed to get all the required fields for a reference node");
                return {};
            }

            // We have some sensible'ish reference so add it to the list
            refs.emplace_back(std::move(reference));
        }
    }

    // Trim if we have a bunch of unused space in the vector
    if (refs.capacity() > (refs.size() + 32))
        refs.shrink_to_fit();

    return refs;
}
