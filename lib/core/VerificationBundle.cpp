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

#include "VerificationBundle.h"
#include "crypto/openssl/VerificationBundleImpl.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

VerificationBundle::VerificationBundle()
    : m_impl(std::make_unique<VerificationBundleImpl>())
{
}

VerificationBundle::VerificationBundle(const VerificationBundle &other)
    : m_impl(other.m_impl->clone())
{
}

VerificationBundle::VerificationBundle(VerificationBundle &&other) noexcept
    : m_impl(std::make_unique<VerificationBundleImpl>())
{
    m_impl.swap(other.m_impl);
}

VerificationBundle::~VerificationBundle() // NOLINT(modernize-use-equals-default)
{
}

VerificationBundle &VerificationBundle::operator=(const VerificationBundle &other)
{
    if (this == &other)
        return *this;

    m_impl = other.m_impl->clone();
    return *this;
}

VerificationBundle &VerificationBundle::operator=(VerificationBundle &&other) noexcept
{
    m_impl->clear();
    m_impl.swap(other.m_impl);
    return *this;
}

void VerificationBundle::addCertificate(const Certificate &certificate)
{
    m_impl->addCertificate(certificate.m_impl);
}

void VerificationBundle::addCertificates(const std::list<Certificate> &certificates)
{
    for (const auto &cert : certificates)
        m_impl->addCertificate(cert.m_impl);
}

void VerificationBundle::setCertificates(const std::list<Certificate> &certificates)
{
    m_impl->clearCertificates();

    for (const auto &cert : certificates)
        m_impl->addCertificate(cert.m_impl);
}

std::list<Certificate> VerificationBundle::certificates() const
{
    std::list<Certificate> certs;

    const auto implCerts = m_impl->certificates();
    for (const std::shared_ptr<ICertificateImpl> &implCert : implCerts)
        certs.emplace_back(Certificate(implCert));

    return certs;
}

bool VerificationBundle::isEmpty() const
{
    return m_impl->isEmpty();
}

void VerificationBundle::clear()
{
    return m_impl->clear();
}

Result<> VerificationBundle::verifyCertificate(const Certificate &certificate,
                                               const std::list<Certificate> &untrustedCerts, VerifyOptions options) const
{
    std::vector<std::shared_ptr<ICertificateImpl>> untrustedCertsImpl;
    untrustedCertsImpl.reserve(untrustedCerts.size());

    for (const auto &untrustedCert : untrustedCerts)
        untrustedCertsImpl.emplace_back(untrustedCert.m_impl);

    Error err;
    if (m_impl->verifyCertificate(certificate.m_impl, untrustedCertsImpl, options, &err) == true)
        return Ok();
    else
        return Error(err);
}
