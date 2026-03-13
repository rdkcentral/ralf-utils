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

#include "Certificate.h"
#include "LibRalf.h"

#include <cinttypes>
#include <string>
#include <vector>

namespace LIBRALF_NS
{

    class ICertificateImpl
    {
    public:
        virtual ~ICertificateImpl() = default;

        virtual std::string subject() const = 0;
        virtual std::string issuer() const = 0;
        virtual std::string commonName() const = 0;

        virtual std::chrono::system_clock::time_point notBefore() const = 0;
        virtual std::chrono::system_clock::time_point notAfter() const = 0;

        virtual std::string toString() const = 0;

        virtual bool isSame(const ICertificateImpl *other) const = 0;

        virtual std::string toPem() const = 0;
        virtual std::vector<uint8_t> toDer() const = 0;
    };

} // namespace LIBRALF_NS