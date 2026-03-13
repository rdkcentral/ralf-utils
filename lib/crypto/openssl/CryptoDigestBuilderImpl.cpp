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

#include "CryptoDigestBuilderImpl.h"
#include "core/LogMacros.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

// -----------------------------------------------------------------------------
/*!
    \static

    This API is used by the dm-verity code which is performance critical, so we
    have a special case to handle sha256 digest calculations on small buffers.
    This is because the OpenSSL EVP API is quite slow for small buffers.

    \see https://github.com/openssl/openssl/issues/19612
 */
std::array<uint8_t, 32> CryptoDigestBuilderImpl::sha256Digest(const void *_Nonnull data, size_t length,
                                                              const void *_Nullable salt, size_t saltSize)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    if (salt && saltSize)
        SHA256_Update(&ctx, salt, saltSize);

    SHA256_Update(&ctx, data, length);

    std::array<uint8_t, 32> digest = {};
    SHA256_Final(digest.data(), &ctx);

#pragma GCC diagnostic pop

    return digest;
}

CryptoDigestBuilderImpl::CryptoDigestBuilderImpl(CryptoDigestBuilder::Algorithm algorithm)
    : m_algorithm(CryptoDigestBuilder::Algorithm::Null)
    , m_context(nullptr)
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
        m_algorithm = algorithm;
    }
}

CryptoDigestBuilderImpl::~CryptoDigestBuilderImpl()
{
    if (m_context)
    {
        EVP_MD_CTX_free(m_context);
        m_context = nullptr;
    }
}

bool CryptoDigestBuilderImpl::initContext(EVP_MD_CTX *context, CryptoDigestBuilder::Algorithm algorithm)
{
    const EVP_MD *md = nullptr;
    switch (algorithm)
    {
        case CryptoDigestBuilder::Algorithm::Null:
            md = EVP_md_null();
            break;

        case CryptoDigestBuilder::Algorithm::Md5:
            md = EVP_md5();
            break;

        case CryptoDigestBuilder::Algorithm::Sha1:
            md = EVP_sha1();
            break;
        case CryptoDigestBuilder::Algorithm::Sha256:
            md = EVP_sha256();
            break;
        case CryptoDigestBuilder::Algorithm::Sha384:
            md = EVP_sha384();
            break;
        case CryptoDigestBuilder::Algorithm::Sha512:
            md = EVP_sha512();
            break;

        default:
            logError("Unknown or unsupported digest algorithm");
            return false;
    }

    return EVP_DigestInit_ex(context, md, nullptr) == 1;
}

CryptoDigestBuilder::Algorithm CryptoDigestBuilderImpl::algorithm() const
{
    return m_algorithm;
}

void CryptoDigestBuilderImpl::update(const void *_Nullable data, size_t length)
{
    if (m_context)
    {
        if (length == 0)
            return;

        if (data == nullptr)
        {
            static const uint8_t zero[512] = { 0 };
            while (length > 0)
            {
                const size_t chunk = std::min(length, sizeof(zero));
                if (EVP_DigestUpdate(m_context, zero, chunk) != 1)
                {
                    logError("Failed to update digest context - %s", ERR_error_string(ERR_get_error(), nullptr));
                    return;
                }

                length -= chunk;
            }
        }
        else
        {
            if (EVP_DigestUpdate(m_context, data, length) != 1)
                logError("Failed to update digest context - %s", ERR_error_string(ERR_get_error(), nullptr));
        }
    }
}

void CryptoDigestBuilderImpl::reset()
{
    if (m_context)
    {
        EVP_MD_CTX_reset(m_context);
        initContext(m_context, m_algorithm);
    }
}

std::vector<uint8_t> CryptoDigestBuilderImpl::finalise() const
{
    if (!m_context)
        return {};

    ERR_clear_error();

    std::vector<uint8_t> digest(EVP_MAX_MD_SIZE);
    unsigned int digestLength = 0;

    if (EVP_DigestFinal_ex(m_context, digest.data(), &digestLength) != 1)
    {
        logError("Failed to finalise digest context - %s", ERR_error_string(ERR_get_error(), nullptr));
        return {};
    }

    digest.resize(digestLength);
    return digest;
}
