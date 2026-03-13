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

#include "Compatibility.h"
#include "LibRalf.h"
#include "Result.h"

#include <cinttypes>
#include <cstdint>
#include <string>
#include <vector>

namespace LIBRALF_NS
{
    class Base64
    {
    public:
        // -------------------------------------------------------------------------
        /*!
            Simply decodes the given \a base64 string into a byte array.

        */
        static Result<std::vector<uint8_t>> decode(const std::string &base64);

        // -------------------------------------------------------------------------
        /*!
            Simply encodes the given \a data to a base64 string.

        */
        static Result<std::string> encode(const std::vector<uint8_t> &data);
    };

} // namespace LIBRALF_NS
