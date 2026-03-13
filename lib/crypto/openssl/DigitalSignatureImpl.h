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

#include "core/IDigitalSignatureImpl.h"

#include <memory>

struct evp_md_ctx_st;
typedef struct evp_md_ctx_st EVP_MD_CTX;

// -----------------------------------------------------------------------------
/**
    \class DigitalSignatureImpl
    \brief Implementation of the digital signature interface using OpenSSL.

    This is a thin wrapper around the following OpenSSL EVP APIs:
        - EVP_DigestInit
        - EVP_DigestUpdate
        - EVP_VerifyFinal

 */
class DigitalSignatureImpl final : public LIBRALF_NS::IDigitalSignatureImpl
{
public:
    explicit DigitalSignatureImpl(LIBRALF_NS::DigitalSignature::Algorithm);
    ~DigitalSignatureImpl() final;

    LIBRALF_NS::DigitalSignature::Algorithm algorithm() const override;

    void update(const void *_Nullable data, size_t length) override;

    void reset() override;

    bool verify(const std::shared_ptr<LIBRALF_NS::ICertificateImpl> &certificate, const void *_Nullable signature,
                size_t signatureLength, LIBRALF_NS::Error *_Nullable error) override;

private:
    static bool initContext(EVP_MD_CTX *_Nonnull context, LIBRALF_NS::DigitalSignature::Algorithm algorithm);

private:
    const LIBRALF_NS::DigitalSignature::Algorithm m_algorithm;

    EVP_MD_CTX *_Nullable m_context = nullptr;
};