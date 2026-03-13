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

#include "Certificate.h"
#include "ICertificateImpl.h"
#include "crypto/openssl/CertificateImpl.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

Result<Certificate> Certificate::loadFromFile(const std::filesystem::path &filePath, EncodingFormat format)
{
    Error err;
    auto cert = CertificateImpl::loadFromFile(filePath, format, &err);
    if (err)
        return Error(err);
    if (!cert)
        return Error(ErrorCode::GenericCryptoError, "Failed to load certificate from file");

    return Certificate(std::move(cert));
}

Result<Certificate> Certificate::loadFromString(std::string_view str)
{
    Error err;
    auto cert = CertificateImpl::loadFromMemory(str.data(), str.length(), EncodingFormat::PEM, &err);
    if (err)
        return Error(err);
    if (!cert)
        return Error(ErrorCode::GenericCryptoError, "Failed to load certificate from string");

    return Certificate(std::move(cert));
}

Result<Certificate> Certificate::loadFromVector(const std::vector<uint8_t> &data, EncodingFormat format)
{
    Error err;
    auto cert = CertificateImpl::loadFromMemory(data.data(), data.size(), format, &err);
    if (err)
        return Error(err);
    if (!cert)
        return Error(ErrorCode::GenericCryptoError, "Failed to load certificate from memory");

    return Certificate(std::move(cert));
}

Result<Certificate> Certificate::loadFromMemory(const void *data, size_t length, EncodingFormat format)
{
    Error err;
    auto cert = CertificateImpl::loadFromMemory(data, length, format, &err);
    if (err)
        return Error(err);
    if (!cert)
        return Error(ErrorCode::GenericCryptoError, "Failed to load certificate from memory");

    return Certificate(std::move(cert));
}

Result<std::list<Certificate>> Certificate::loadFromFileMultiCerts(const std::filesystem::path &filePath)
{
    Error err;
    const auto certs = CertificateImpl::loadFromFileMultiCerts(filePath, &err);
    if (err)
        return Error(err);
    if (certs.empty())
        return Error(ErrorCode::GenericCryptoError, "Failed to load certificate(s) from file");

    std::list<Certificate> results;
    for (const auto &cert : certs)
    {
        results.emplace_back(Certificate(cert));
    }

    return results;
}

Result<std::list<Certificate>> Certificate::loadFromStringMultiCerts(std::string_view str)
{
    Error err;
    const auto certs = CertificateImpl::loadFromMemoryMultiCerts(str.data(), str.length(), &err);
    if (err)
        return Error(err);
    if (certs.empty())
        return Error(ErrorCode::GenericCryptoError, "Failed to load certificate(s) from string");

    std::list<Certificate> results;
    for (const auto &cert : certs)
    {
        results.emplace_back(Certificate(cert));
    }

    return results;
}

Certificate::Certificate() // NOLINT(modernize-use-equals-default)
{
}

Certificate::Certificate(std::shared_ptr<ICertificateImpl> impl)
    : m_impl(std::move(impl))
{
}

Certificate::Certificate(const Certificate &other) // NOLINT(modernize-use-equals-default)
    : m_impl(other.m_impl)
{
}

Certificate::Certificate(Certificate &&other) noexcept
    : m_impl(std::move(other.m_impl))
{
}

Certificate::~Certificate() // NOLINT(modernize-use-equals-default)
{
}

Certificate &Certificate::operator=(Certificate &&other) noexcept // NOLINT(modernize-use-equals-default)
{
    m_impl = std::move(other.m_impl);
    return *this;
}

Certificate &Certificate::operator=(const Certificate &other) // NOLINT(modernize-use-equals-default)
{
    if (this != &other)
        m_impl = other.m_impl;

    return *this;
}

bool Certificate::operator!=(const Certificate &other) const
{
    if (!m_impl || !other.m_impl)
        return true;
    else
        return !m_impl->isSame(other.m_impl.get());
}

bool Certificate::operator==(const Certificate &other) const
{
    if (!m_impl || !other.m_impl)
        return false;
    else
        return m_impl->isSame(other.m_impl.get());
}

bool Certificate::isNull() const
{
    return m_impl == nullptr;
}

bool Certificate::isValid() const
{
    return m_impl != nullptr;
}

std::string Certificate::subject() const
{
    if (m_impl)
        return m_impl->subject();
    else
        return {};
}

std::string Certificate::issuer() const
{
    if (m_impl)
        return m_impl->issuer();
    else
        return {};
}

std::string Certificate::commonName() const
{
    if (m_impl)
        return m_impl->commonName();
    else
        return {};
}

std::chrono::system_clock::time_point Certificate::notBefore() const
{
    if (m_impl)
        return m_impl->notBefore();
    else
        return {};
}

std::chrono::system_clock::time_point Certificate::notAfter() const
{
    if (m_impl)
        return m_impl->notAfter();
    else
        return {};
}

std::string Certificate::toString() const
{
    if (m_impl)
        return m_impl->toString();
    else
        return "<null>";
}

std::ostream &LIBRALF_NS::operator<<(std::ostream &s, const Certificate &cert)
{
    if (!cert.m_impl)
        return s << "Certificate: <null>";
    else
        return s << cert.m_impl->toString();
}