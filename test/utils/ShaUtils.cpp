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

#include "ShaUtils.h"

#include <openssl/evp.h>
#include <openssl/sha.h>

std::array<uint8_t, 32> sha256Sum(const void *data, size_t dataLen)
{
    std::array<uint8_t, SHA256_DIGEST_LENGTH> digest = {};

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data, dataLen);

    unsigned int digestLen = SHA256_DIGEST_LENGTH;
    EVP_DigestFinal_ex(ctx, digest.data(), &digestLen);
    EVP_MD_CTX_free(ctx);

    return digest;
}
