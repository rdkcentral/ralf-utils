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

#include "Base64.h"
#include "crypto/openssl/Base64Impl.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

Result<std::vector<uint8_t>> Base64::decode(const std::string &base64)
{
    return Base64Impl::decode(base64);
}

Result<std::string> Base64::encode(const std::vector<uint8_t> &data)
{
    return Base64Impl::encode(data.data(), data.size());
}