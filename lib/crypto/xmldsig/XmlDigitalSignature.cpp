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

#include "XmlDigitalSignature.h"
#include "XmlDigitalSignatureImpl.h"

#include <libxml/parser.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

XmlDigitalSignature XmlDigitalSignature::parse(const char *signatureXml, size_t signatureXmlLength,
                                               LIBRALF_NS::Error *_Nullable error)
{
    // Parse the xml document
    auto *doc = xmlReadMemory(signatureXml, static_cast<int>(signatureXmlLength), "", "", 0);
    if (!doc)
    {
        if (error)
            error->assign(LIBRALF_NS::ErrorCode::XmlParseError, "Failed to parse the signature XML");
        return {};
    }

    // Wrap the document in a shared implementation
    auto impl = XmlDigitalSignatureImpl::createImpl(doc, error);
    if (!impl)
    {
        xmlFreeDoc(doc);

        // Error already set by the schema validator
        return {};
    }

    return XmlDigitalSignature(std::move(impl));
}

XmlDigitalSignature::XmlDigitalSignature() // NOLINT(modernize-use-equals-default)
{
}

XmlDigitalSignature::XmlDigitalSignature(std::shared_ptr<XmlDigitalSignatureImpl> &&impl) // NOLINT(modernize-use-equals-default)
    : m_impl(std::move(impl))
{
}

XmlDigitalSignature::XmlDigitalSignature(const XmlDigitalSignature &other) // NOLINT(modernize-use-equals-default)
    : m_impl(other.m_impl)
{
}

XmlDigitalSignature::XmlDigitalSignature(XmlDigitalSignature &&other) noexcept // NOLINT(modernize-use-equals-default)
    : m_impl(std::move(other.m_impl))
{
}

XmlDigitalSignature::~XmlDigitalSignature() // NOLINT(modernize-use-equals-default)
{
}

XmlDigitalSignature &XmlDigitalSignature::operator=(const XmlDigitalSignature &other) // NOLINT(modernize-use-equals-default)
{
    if (this != &other)
        m_impl = other.m_impl;

    return *this;
}

XmlDigitalSignature &XmlDigitalSignature::operator=(XmlDigitalSignature &&other) noexcept
{
    m_impl = std::move(other.m_impl);
    return *this;
}

bool XmlDigitalSignature::isNull() const
{
    return m_impl == nullptr;
}

bool XmlDigitalSignature::verify(const VerificationBundle &bundle, VerifyOptions options, Error *_Nullable error) const
{
    if (!m_impl)
    {
        if (error)
            error->assign(ErrorCode::InvalidArgument, "Invalid signature document");

        return false;
    }

    return m_impl->verify(bundle, options, error);
}

std::vector<XmlDigitalSignature::Reference> XmlDigitalSignature::references(Error *_Nullable error) const
{
    if (!m_impl)
    {
        if (error)
            error->assign(ErrorCode::InvalidArgument, "Invalid signature document");

        return {};
    }

    return m_impl->references(error);
}

std::list<Certificate> XmlDigitalSignature::signingCertificates(Error *_Nullable error) const
{
    if (!m_impl)
    {
        if (error)
            error->assign(ErrorCode::InvalidArgument, "Invalid signature document");

        return {};
    }

    return m_impl->signingCertificates(error);
}
