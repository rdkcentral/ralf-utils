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

#include "CryptoUtils.h"

std::string getLastOpenSSLError()
{
    auto bio = BIOUniquePtr(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio)
        return {};

    ERR_print_errors(bio.get());

    char *buf = nullptr;
    size_t len = BIO_get_mem_data(bio.get(), &buf);
    if (!len || !buf)
        return {};

    return { buf, len };
}
