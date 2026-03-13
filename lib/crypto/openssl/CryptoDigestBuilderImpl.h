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

#include "core/ICryptoDigestBuilderImpl.h"

#include <memory>

struct evp_md_ctx_st;
typedef struct evp_md_ctx_st EVP_MD_CTX;

class CryptoDigestBuilderImpl final : public LIBRALF_NS::ICryptoDigestBuilderImpl
{
public:
    static std::array<uint8_t, 32> sha256Digest(const void *_Nonnull data, size_t length, const void *_Nullable salt,
                                                size_t saltSize);

public:
    explicit CryptoDigestBuilderImpl(LIBRALF_NS::CryptoDigestBuilder::Algorithm algorithm);
    ~CryptoDigestBuilderImpl() final;

    LIBRALF_NS::CryptoDigestBuilder::Algorithm algorithm() const override;

    void update(const void *_Nullable data, size_t length) override;

    void reset() override;

    std::vector<uint8_t> finalise() const override;

private:
    static bool initContext(EVP_MD_CTX *_Nonnull context, LIBRALF_NS::CryptoDigestBuilder::Algorithm algorithm);

    LIBRALF_NS::CryptoDigestBuilder::Algorithm m_algorithm;
    EVP_MD_CTX *_Nullable m_context;
};
