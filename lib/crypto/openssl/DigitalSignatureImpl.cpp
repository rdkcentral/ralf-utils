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

#include "DigitalSignatureImpl.h"
#include "CertificateImpl.h"
#include "CryptoUtils.h"
#include "core/LogMacros.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

DigitalSignatureImpl::DigitalSignatureImpl(DigitalSignature::Algorithm algorithm)
    : m_algorithm(algorithm)
{
    ERR_clear_error();

    m_context = EVP_MD_CTX_new();
    if (!m_context)
    {
        logError("Failed to create digest context - %s", ERR_error_string(ERR_get_error(), nullptr));
    }
    else if (!initContext(m_context, algorithm))
    {
        logError("Failed to initialise digest context - %s", ERR_error_string(ERR_get_error(), nullptr));

        EVP_MD_CTX_free(m_context);
        m_context = nullptr;
    }
    else
    {
        logDebug("Created digest context for algorithm %d", static_cast<int>(algorithm));
    }
}

DigitalSignatureImpl::~DigitalSignatureImpl()
{
    if (m_context)
    {
        EVP_MD_CTX_free(m_context);
        m_context = nullptr;
    }
}

bool DigitalSignatureImpl::initContext(EVP_MD_CTX *context, LIBRALF_NS::DigitalSignature::Algorithm algorithm)
{
    const EVP_MD *md = nullptr;
    switch (algorithm)
    {
        case DigitalSignature::Algorithm::Null:
            md = EVP_md_null();
            break;
        case DigitalSignature::Algorithm::Sha1:
            md = EVP_sha1();
            break;
        case DigitalSignature::Algorithm::Sha224:
            md = EVP_sha224();
            break;
        case DigitalSignature::Algorithm::Sha256:
            md = EVP_sha256();
            break;
        case DigitalSignature::Algorithm::Sha384:
            md = EVP_sha384();
            break;
        case DigitalSignature::Algorithm::Sha512:
            md = EVP_sha512();
            break;

        default:
            logError("Unsupported algorithm %d", static_cast<int>(algorithm));
            return false;
    }

    if (!md)
    {
        logError("Failed to get the digest algorithm for %d", static_cast<int>(algorithm));
        return false;
    }

    if (EVP_DigestInit(context, md) != 1)
    {
        logError("Failed to initialise the digest context for algorithm %d", static_cast<int>(algorithm));
        return false;
    }

    return true;
}

DigitalSignature::Algorithm DigitalSignatureImpl::algorithm() const
{
    return m_algorithm;
}

void DigitalSignatureImpl::update(const void *data, size_t length)
{
    if (!m_context)
    {
        logError("Digest context is not initialised");
    }
    else
    {
        if (EVP_DigestUpdate(m_context, data, length) != 1)
            logError("Failed to update digest context - %s", ERR_error_string(ERR_get_error(), nullptr));
    }
}

void DigitalSignatureImpl::reset()
{
    if (!m_context)
    {
        logError("Digest context is not initialised");
    }
    else if (!initContext(m_context, m_algorithm))
    {
        logError("Failed to reset digest context - %s", ERR_error_string(ERR_get_error(), nullptr));
    }
}

bool DigitalSignatureImpl::verify(const std::shared_ptr<ICertificateImpl> &certificate, const void *signature,
                                  size_t signatureLength, Error *_Nullable error)
{
    if (!m_context)
    {
        if (error)
            error->assign(ErrorCode::GenericCryptoError, "Digest context is not initialised");

        logError("Digest context is not initialised");
        return false;
    }

    // Get the certificate object implementation
    auto certImpl = std::dynamic_pointer_cast<CertificateImpl>(certificate);
    if (!certImpl)
    {
        if (error)
            error->assign(ErrorCode::InvalidArgument, "Invalid certificate object");

        logError("Invalid certificate object");
        return false;
    }

    // Extract the public key from the certificate
    auto publicKey = EVP_PKEYUniquePtr(X509_get_pubkey(certImpl->x509Cert()), EVP_PKEY_free);
    if (!publicKey)
    {
        if (error)
            error->assign(ErrorCode::GenericCryptoError,
                          "Failed to extract the public key from the signing certificate");

        return false;
    }

    // Perform the verification
    if (EVP_VerifyFinal(m_context, reinterpret_cast<const unsigned char *>(signature), signatureLength,
                        publicKey.get()) != 1)
    {
        if (error)
            *error = Error::format(ErrorCode::GenericCryptoError, "Failed to verify the signature - %s",
                                   getLastOpenSSLError().c_str());

        return false;
    }

    return true;
}
