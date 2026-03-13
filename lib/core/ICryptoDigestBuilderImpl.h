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

#include "CryptoDigestBuilder.h"

#include <cinttypes>
#include <cstdint>
#include <vector>

namespace LIBRALF_NS
{
    class ICryptoDigestBuilderImpl
    {
    public:
        virtual ~ICryptoDigestBuilderImpl() = default;

        virtual CryptoDigestBuilder::Algorithm algorithm() const = 0;

        virtual void update(const void *_Nullable data, size_t length) = 0;

        virtual void reset() = 0;

        virtual std::vector<uint8_t> finalise() const = 0;
    };

} // namespace LIBRALF_NS
