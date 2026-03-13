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

#include "OCIUtils.h"

static inline uint8_t hexDigitToByte(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

std::vector<uint8_t> hexStringToBytes(std::string_view hex)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);

    for (size_t i = 0; i < (hex.size() / 2); i++)
    {
        const char high = hex[(i * 2)];
        const char low = hex[(i * 2) + 1];

        if (!isxdigit(high) || !isxdigit(low))
            break;

        const uint8_t byte = (hexDigitToByte(high) << 4) | hexDigitToByte(low);
        bytes.push_back(byte);
    }

    return bytes;
}

bool validateSha256Digest(std::string_view digest)
{
    if (digest.size() != 64)
        return false;

    for (const char c : digest)
    {
        if (!isxdigit(c))
            return false;
    }

    return true;
}

std::optional<std::string> validateDigest(std::string_view digest)
{
    if (digest.size() != 71)
        return std::nullopt;
    if (digest.find("sha256:") != 0)
        return std::nullopt;

    // remove the "sha256:" prefix
    std::string_view stripped = digest.substr(7);
    if (!validateSha256Digest(stripped))
        return std::nullopt;

    return std::string(stripped);
}
