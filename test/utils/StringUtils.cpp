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

#include "StringUtils.h"

#include <openssl/bio.h>
#include <openssl/evp.h>

std::string toHex(const uint8_t *data, size_t length)
{
    static const char hex[] = "0123456789abcdef";

    std::string hexStr;
    hexStr.reserve(length * 2);

    for (size_t i = 0; i < length; i++)
    {
        hexStr.push_back(hex[(data[i] >> 4) & 0xf]);
        hexStr.push_back(hex[(data[i] >> 0) & 0xf]);
    }

    return hexStr;
}

std::vector<uint8_t> fromHex(const char *hex, size_t length)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(length / 2);

    for (size_t i = 0; i < length; i += 2)
    {
        uint8_t byte = 0;
        for (size_t j = 0; j < 2; j++)
        {
            byte <<= 4;
            if ((hex[i + j] >= '0') && (hex[i + j] <= '9'))
                byte |= hex[i + j] - '0';
            else if ((hex[i + j] >= 'a') && (hex[i + j] <= 'f'))
                byte |= hex[i + j] - 'a' + 10;
            else if ((hex[i + j] >= 'A') && (hex[i + j] <= 'F'))
                byte |= hex[i + j] - 'A' + 10;
            else
                return {};
        }

        bytes.push_back(byte);
    }

    return bytes;
}

std::vector<uint8_t> fromBase64(const char *base64, size_t length)
{
    BIO *bio, *b64;
    std::vector<uint8_t> buffer(length);

    bio = BIO_new_mem_buf(base64, static_cast<int>(length));
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    // Do not use newlines to flush buffer
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    int len = BIO_read(bio, buffer.data(), static_cast<int>(buffer.size()));
    buffer.resize(len);

    BIO_free_all(bio);
    return buffer;
}
