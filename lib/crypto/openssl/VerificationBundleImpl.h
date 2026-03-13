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

#include "CertificateImpl.h"
#include "EnumFlags.h"
#include "core/IVerificationBundleImpl.h"

#include <functional>

// Forward declarations to avoid exposing the OpenSSL headers to rest of the library
struct x509_store_st;
typedef struct x509_store_st X509_STORE;

class VerificationBundleImpl final : public LIBRALF_NS::IVerificationBundleImpl
{
public:
    ~VerificationBundleImpl() override = default;

    std::unique_ptr<IVerificationBundleImpl> clone() override;

    void addCertificate(const std::shared_ptr<LIBRALF_NS::ICertificateImpl> &certificate) override;
    void clearCertificates() final;

    std::list<std::shared_ptr<LIBRALF_NS::ICertificateImpl>> certificates() const override;

    // Future work: add support for adding CRLs to the bundle

    bool isEmpty() const override;

    void clear() override;

    bool verifyPkcs7(const void *_Nonnull pkcs7Blob, size_t pkcs7BlobSize,
                     LIBRALF_NS::VerificationBundle::VerifyOptions options, std::vector<uint8_t> *_Nullable contents,
                     LIBRALF_NS::Error *_Nullable error) const override;

    bool verifyCertificate(const std::shared_ptr<LIBRALF_NS::ICertificateImpl> &certificate,
                           const std::vector<std::shared_ptr<LIBRALF_NS::ICertificateImpl>> &untrustedCerts,
                           LIBRALF_NS::VerificationBundle::VerifyOptions options,
                           LIBRALF_NS::Error *_Nullable error) const override;

private:
    using X509UniqueSmartPtr = std::unique_ptr<X509_STORE, std::function<void(X509_STORE *_Nonnull)>>;
    X509UniqueSmartPtr createCaStore(LIBRALF_NS::VerificationBundle::VerifyOptions options,
                                     LIBRALF_NS::Error *_Nullable error) const;

private:
    std::list<std::shared_ptr<CertificateImpl>> m_certificates;
};
