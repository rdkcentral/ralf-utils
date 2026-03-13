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

#include "VerificationBundleImpl.h"
#include "CertificateImpl.h"
#include "CryptoUtils.h"
#include "core/LogMacros.h"

#include <openssl/bio.h>
#include <openssl/pkcs7.h>
#include <openssl/sha.h>
#include <openssl/x509v3.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

#define MAX_PKCS7_BLOB_SIZE (1024 * 1024)
#define MAX_PKCS7_CONTENT_SIZE (1024 * 1024)

std::unique_ptr<IVerificationBundleImpl> VerificationBundleImpl::clone()
{
    auto bundle = std::make_unique<VerificationBundleImpl>();

    for (const auto &cert : m_certificates)
        bundle->addCertificate(cert);

    return bundle;
}

bool VerificationBundleImpl::isEmpty() const
{
    return m_certificates.empty();
}

void VerificationBundleImpl::clear()
{
    m_certificates.clear();
}

void VerificationBundleImpl::addCertificate(const std::shared_ptr<ICertificateImpl> &certificate)
{
    m_certificates.emplace_back(std::static_pointer_cast<CertificateImpl>(certificate));
}

void VerificationBundleImpl::clearCertificates()
{
    m_certificates.clear();
}

std::list<std::shared_ptr<ICertificateImpl>> VerificationBundleImpl::certificates() const
{
    std::list<std::shared_ptr<ICertificateImpl>> certs;

    for (const auto &cert : m_certificates)
        certs.push_back(cert);

    return certs;
}

// -----------------------------------------------------------------------------
/*!
    Creates an OpenSSL X509_STORE object containing all the certificates stored
    in this object.

 */
X509StoreUniquePtr VerificationBundleImpl::createCaStore(VerificationBundle::VerifyOptions options,
                                                         Error *_Nullable error) const
{
    ERR_clear_error();

    X509StoreUniquePtr caStore(X509_STORE_new(), X509_STORE_free);

    for (const auto &cert : m_certificates)
    {
        if (!X509_STORE_add_cert(caStore.get(), cert->x509Cert()))
        {
            if (error)
                error->assign(ErrorCode::GenericCryptoError, "Failed to add certificate to CA store");

            return nullptr;
        }

        logDebug("Added certificate to CA store: %s", cert->subject().c_str());
    }

    // Future work: add support for adding CRLs to the bundle

    // Set the ignore expiry time on the certs in the store
    X509_VERIFY_PARAM *param = X509_STORE_get0_param(caStore.get());
    if (!param)
    {
        if (error)
            error->assign(ErrorCode::GenericCryptoError, "Failed to get the X509 store verify parameters");

        // If we've failed here we don't want to limp along and then verify against current time, because that causes
        // an expiry time-bomb if no one notices the above error message ... lessons learnt
        return nullptr;
    }
    else
    {
        int rc;
        if ((options & VerificationBundle::VerifyOption::CheckCertificateExpiry) ==
            VerificationBundle::VerifyOption::CheckCertificateExpiry)
            rc = X509_VERIFY_PARAM_clear_flags(param, X509_V_FLAG_NO_CHECK_TIME);
        else
            rc = X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_NO_CHECK_TIME);

        if (rc == 0)
        {
            if (error)
                error->assign(ErrorCode::GenericCryptoError, "Failed to set / clear X509_V_FLAG_NO_CHECK_TIME flag");

            return nullptr;
        }

        // TODO: check if need to change the purpose to, see the following link for why this may be important
        // https://technotes.shemyak.com/posts/smimesign-extended-key-usage-extension-for-openssl-pkcs7-verification/
        // For now though, going to require the certificates used for signing nad verification have the correct purpose
        // set.
        // X509_VERIFY_PARAM_set_purpose(param, X509_PURPOSE_ANY);
    }

    return caStore;
}

// -----------------------------------------------------------------------------
/*!
    Perform a PKCS7 verification on the supplied PKCS7 blob using the certificates
    stored in the bundle as a trusted CA store.

    If \a contents is not nullptr then the contents of the PKCS7 blob will be
    returned.

    Returns \c true if the verification was successful, \c false otherwise.

 */
bool VerificationBundleImpl::verifyPkcs7(const void *pkcs7Blob, size_t pkcs7BlobSize,
                                         VerificationBundle::VerifyOptions options,
                                         std::vector<uint8_t> *_Nullable contents, Error *_Nullable error) const
{
    if (error)
        error->clear();
    if (contents)
        contents->clear();

    if ((pkcs7Blob == nullptr) || (pkcs7BlobSize == 0 || pkcs7BlobSize > MAX_PKCS7_BLOB_SIZE))
    {
        if (error)
            error->assign(ErrorCode::InvalidArgument, "Invalid PKCS7 blob");

        return false;
    }

    ERR_clear_error();

    // Create a CA store with all the certificates
    // TODO: should we generate this store on construction then just re-use it here?
    auto caStore = createCaStore(options, error);
    if (!caStore)
    {
        // Error object already updated
        return false;
    }

    // Load content into an openssl buffer
    auto bioIn = BIOUniquePtr(BIO_new_mem_buf(pkcs7Blob, static_cast<int>(pkcs7BlobSize)), BIO_free);
    if (!bioIn)
    {
        if (error)
        {
            *error = Error::format(ErrorCode::GenericCryptoError, "BIO_new_mem_buf failed for pkcs7 data of size %zu",
                                   pkcs7BlobSize);
        }

        return false;
    }

    // And parse the PKCS7 data
    auto pkcs7 = PKCS7UniquePtr(d2i_PKCS7_bio(bioIn.get(), nullptr), PKCS7_free);
    if (!pkcs7)
    {
        if (error)
            error->assign(ErrorCode::GenericCryptoError, "Failed to parse the PKCS7 DER formatted signature");

        return false;
    }

    // Create a memory buffer to store the (verified) output, ie. the SHA256 in text form
    BIOUniquePtr bioOut;
    if (contents)
    {
        bioOut = BIOUniquePtr(BIO_new(BIO_s_mem()), BIO_free);
        if (!bioOut)
        {
            if (error)
                error->assign(ErrorCode::GenericCryptoError, "Failed to create memory bio to store PKCS7 contents");

            return false;
        }
    }

    int verifyFlags = PKCS7_NO_DUAL_CONTENT;

    // Verify the signature
    if (PKCS7_verify(pkcs7.get(), nullptr, caStore.get(), nullptr, bioOut.get(), verifyFlags) != 1)
    {
        if (error)
            *error =
                Error::format(ErrorCode::GenericCryptoError, "Verification failed - %s", getLastOpenSSLError().c_str());

        return false;
    }

    // Extract the verifier data
    if (contents)
    {
        char *contentPtr = nullptr;
        long contentLen = BIO_get_mem_data(bioOut.get(), &contentPtr);
        if (!contentPtr || (contentLen < 0) || (contentLen > MAX_PKCS7_CONTENT_SIZE))
        {
            if (error)
                error->assign(ErrorCode::GenericCryptoError, "The verified pkcs7 data is invalid or to big");

            return false;
        }

        *contents = std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(contentPtr),
                                         reinterpret_cast<const uint8_t *>(contentPtr + contentLen));
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    Verifies the \a certificate using the certificates stored in the bundle as
    a trusted CA store. \a untrustedCerts is option and can be used to provide
    a chain of certificates that are not trusted but are used to build the
    chain of trust.

 */
bool VerificationBundleImpl::verifyCertificate(const std::shared_ptr<ICertificateImpl> &certificate,
                                               const std::vector<std::shared_ptr<ICertificateImpl>> &untrustedCerts,
                                               VerificationBundle::VerifyOptions options, Error *_Nullable error) const
{
    if (error)
        error->clear();

    ERR_clear_error();

    // Sanity check
    auto targetImpl = std::static_pointer_cast<CertificateImpl>(certificate);
    if (!targetImpl)
    {
        if (error)
            error->assign(ErrorCode::InvalidArgument, "No certificates to verify");

        return false;
    }

    // Parse the target certificate we're validating
    logDebug("Signing cert: issuer:'%s' subject:'%s'", targetImpl->issuer().c_str(), targetImpl->subject().c_str());

    // Create a stack of any optional intermediate certificates, the untrusted chain is allowed to be null when
    // feed into the OpenSSL verify function
    X509StackUniquePtr untrustedChain;
    if (!untrustedCerts.empty())
    {
        untrustedChain = X509StackUniquePtr(sk_X509_new_null(), [](STACK_OF(X509) * stX509) { sk_X509_free(stX509); });
        if (!untrustedChain)
        {
            if (error)
                error->assign(ErrorCode::GenericCryptoError, "Failed to create a stack of X509 certificates");

            return false;
        }

        for (const auto &untrustedCert : untrustedCerts)
        {
            const auto &cert = std::static_pointer_cast<CertificateImpl>(untrustedCert);
            logDebug("Untrusted cert: issuer:'%s' subject:'%s'", cert->issuer().c_str(), cert->subject().c_str());

            if (!sk_X509_push(untrustedChain.get(), cert->x509Cert()))
            {
                if (error)
                    error->assign(ErrorCode::GenericCryptoError, "Failed to add certificate to the untrusted chain");

                return false;
            }
        }
    }

    // Create a CA store with all the certificates
    // TODO: should we generate this store on construction then just re-use it here?
    auto caStore = createCaStore(options, error);
    if (!caStore)
    {
        // Error object already updated
        return false;
    }

    // Create a new context for verifying the certificate chain
    auto verifyContext = X509StoreCtxUniquePtr(X509_STORE_CTX_new(), X509_STORE_CTX_free);
    if (!verifyContext)
    {
        if (error)
            error->assign(ErrorCode::GenericCryptoError, "Failed to create a new X509 store context");

        return false;
    }

    // Future work: add support for adding CRLs to the bundle

    // Initialise the context with the target certificate, the untrusted chain and the CA store
    if (X509_STORE_CTX_init(verifyContext.get(), caStore.get(), targetImpl->x509Cert(), untrustedChain.get()) != 1)
    {
        if (error)
            error->assign(ErrorCode::GenericCryptoError, "Failed to initialise X509 store context");

        return false;
    }

    // Perform the verification
    int rc = X509_verify_cert(verifyContext.get());
    bool ok = (rc > 0) && (X509_STORE_CTX_get_error(verifyContext.get()) == X509_V_OK);
    if (!ok)
    {
        const int verifyError = X509_STORE_CTX_get_error(verifyContext.get());
        logError("Failed to verify certificate chain due to '%s'", X509_verify_cert_error_string(verifyError));

        if (error)
        {
            if (verifyError == X509_V_ERR_CERT_HAS_EXPIRED)
                *error = Error::format(ErrorCode::CertificateExpired, "Certificate has expired");
            else if (verifyError == X509_V_ERR_CERT_REVOKED)
                *error = Error::format(ErrorCode::CertificateRevoked, "Certificate has been revoked");
            else if (verifyError == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)
                *error = Error::format(ErrorCode::CertificateError,
                                       "Certificate chain doesn't match any CA certificates in the bundle");
            else
                *error = Error::format(ErrorCode::CertificateError, "Failed to verify certificate chain - %s",
                                       X509_verify_cert_error_string(verifyError));
        }
    }

    // Probably not needed, but just ensure we destroy the verify context first
    verifyContext.reset();
    untrustedChain.reset();
    caStore.reset();

    return ok;
}
