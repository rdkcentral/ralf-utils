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

#include "CryptoDigestBuilder.h"
#include "crypto/openssl/CryptoDigestBuilderImpl.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

CryptoDigestBuilder::CryptoDigestBuilder(Algorithm algorithm)
    : m_impl(std::make_unique<CryptoDigestBuilderImpl>(algorithm))
{
}

CryptoDigestBuilder::~CryptoDigestBuilder() // NOLINT(modernize-use-equals-default)
{
}

CryptoDigestBuilder::CryptoDigestBuilder(CryptoDigestBuilder &&other) noexcept
    : m_impl(std::move(other.m_impl))
{
    other.m_impl = std::make_unique<CryptoDigestBuilderImpl>(m_impl->algorithm());
}

CryptoDigestBuilder &CryptoDigestBuilder::operator=(CryptoDigestBuilder &&other) noexcept
{
    if (this != &other)
    {
        m_impl = std::move(other.m_impl);
        other.m_impl = std::make_unique<CryptoDigestBuilderImpl>(m_impl->algorithm());
    }

    return *this;
}

void CryptoDigestBuilder::update(const void *_Nullable data, size_t length)
{
    m_impl->update(data, length);
}

void CryptoDigestBuilder::reset()
{
    m_impl->reset();
}

std::vector<uint8_t> CryptoDigestBuilder::finalise() const
{
    return m_impl->finalise();
}

std::vector<uint8_t> CryptoDigestBuilder::digest(Algorithm algorithm, const void *_Nullable data, size_t length)
{
    CryptoDigestBuilderImpl impl(algorithm);
    impl.update(data, length);
    return impl.finalise();
}

std::array<uint8_t, 32> CryptoDigestBuilder::sha256Digest(const void *_Nullable data, size_t length,
                                                          const void *_Nullable salt, size_t saltSize)
{
    return CryptoDigestBuilderImpl::sha256Digest(data, length, salt, saltSize);
}
