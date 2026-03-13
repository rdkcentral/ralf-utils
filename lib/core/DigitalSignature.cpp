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

#include "DigitalSignature.h"
#include "crypto/openssl/DigitalSignatureImpl.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

DigitalSignature::DigitalSignature(Algorithm algorithm)
    : m_impl(std::make_unique<DigitalSignatureImpl>(algorithm))
{
}

DigitalSignature::~DigitalSignature() // NOLINT(modernize-use-equals-default)
{
}

DigitalSignature::DigitalSignature(DigitalSignature &&other) noexcept
    : m_impl(std::move(other.m_impl))
{
    other.m_impl = std::make_unique<DigitalSignatureImpl>(m_impl->algorithm());
}

DigitalSignature &DigitalSignature::operator=(DigitalSignature &&other) noexcept
{
    if (this != &other)
    {
        m_impl = std::move(other.m_impl);
        other.m_impl = std::make_unique<DigitalSignatureImpl>(m_impl->algorithm());
    }

    return *this;
}

void DigitalSignature::update(const void *data, size_t length)
{
    m_impl->update(data, length);
}

void DigitalSignature::reset()
{
    m_impl->reset();
}

bool DigitalSignature::verify(const Certificate &certificate, const void *signature, size_t signatureLength,
                              Error *_Nullable error) const
{
    return m_impl->verify(certificate.m_impl, signature, signatureLength, error);
}
