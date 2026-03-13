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

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// -------------------------------------------------------------------------
/*!
    Converts a string of hex digits into a byte array. The conversion stops
    when the end of the string is reached or the first non-hex character is
    found.

*/
std::vector<uint8_t> hexStringToBytes(std::string_view hex);

// -------------------------------------------------------------------------
/*!
    A simple check that the supplied string starts contains exactly 64 hex
    digits

 */
bool validateSha256Digest(std::string_view digest);

// -------------------------------------------------------------------------
/*!
    A simple check that the supplied string starts with sha256: and that
    the remaining string is a valid hex string of the correct length.

    Returns just the has string without the sha256: prefix if valid, otherwise
    returns std::nullopt.

 */
std::optional<std::string> validateDigest(std::string_view digest);
