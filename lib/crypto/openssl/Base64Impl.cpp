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

#include "Base64Impl.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include <algorithm>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

static Result<std::vector<uint8_t>> base64Decode(const std::string &base64)
{
    ERR_clear_error();

    BIO *b64 = BIO_new(BIO_f_base64());
    if (!b64)
        return Error(ErrorCode::GenericCryptoError, "Failed to create BIO base64 filter");

    BIO *bio = BIO_new_mem_buf(base64.data(), static_cast<int>(base64.size()));
    if (!bio)
    {
        BIO_free(b64);
        return Error(ErrorCode::GenericCryptoError, "Failed to create memory buffer BIO for base64 data");
    }

    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    std::vector<uint8_t> decoded(base64.size());
    int decodedLength = BIO_read(bio, decoded.data(), static_cast<int>(base64.size()));
    BIO_free_all(bio);

    if (decodedLength < 0)
        return Error(ErrorCode::GenericCryptoError, "Failed to create memory buffer BIO for base64 data");

    decoded.resize(decodedLength);
    return decoded;
}

Result<std::vector<uint8_t>> Base64Impl::decode(const std::string &base64)
{
    // Remove all whitespace, including newlines, from the string
    std::string trimmedBase64 = base64;
    trimmedBase64.erase(std::remove_if(trimmedBase64.begin(), trimmedBase64.end(),
                                       [](unsigned char c) { return std::isspace(c); }),
                        trimmedBase64.end());

    // Base64 decode the string the sanitised string
    return base64Decode(trimmedBase64);
}

Result<std::string> Base64Impl::encode(const void *data, size_t size)
{
    ERR_clear_error();

    BIO *b64 = BIO_new(BIO_f_base64());
    if (!b64)
        return Error(ErrorCode::GenericCryptoError, "Failed to create BIO base64 filter");

    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio)
    {
        BIO_free(b64);
        return Error(ErrorCode::GenericCryptoError, "Failed to create memory buffer BIO for base64 data");
    }

    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    if (BIO_write(bio, data, static_cast<int>(size)) <= 0)
    {
        BIO_free_all(bio);
        return Error(ErrorCode::GenericCryptoError, "Failed to write data for base64 encoding");
    }

    if (BIO_flush(bio) <= 0)
    {
        BIO_free_all(bio);
        return Error(ErrorCode::GenericCryptoError, "Failed to flash bio used for base64 encoding");
    }

    BUF_MEM *p = nullptr;
    BIO_get_mem_ptr(bio, &p);
    if (!p || !p->data || !p->length)
    {
        BIO_free_all(bio);
        return Error(ErrorCode::GenericCryptoError, "Failed to get base64 encoded string");
    }

    std::string encodedData(reinterpret_cast<const char *>(p->data), p->length);

    BIO_free_all(bio);

    return Ok(encodedData);
}
