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
#include "EnumFlags.h"
#include "LibRalf.h"

#include <filesystem>
#include <list>
#include <memory>

namespace LIBRALF_NS
{
    class IVerificationBundleImpl;

    // -------------------------------------------------------------------------
    /*!
        \class VerificationBundle
        \brief An object that contains a collection of root CA certificates.

        This object is used to hold a collection of root CA certificates that can
        be used to verify the authenticity of packages.  In addition it contains
        some utility methods to verify a certificate against the collection.

     */
    class LIBRALF_EXPORT VerificationBundle
    {
    public:
        VerificationBundle();
        VerificationBundle(const VerificationBundle &other);
        VerificationBundle(VerificationBundle &&other) noexcept;
        ~VerificationBundle();

        VerificationBundle &operator=(const VerificationBundle &other);
        VerificationBundle &operator=(VerificationBundle &&other) noexcept;

        void addCertificate(const Certificate &certificate);
        void addCertificates(const std::list<Certificate> &certificates);

        void setCertificates(const std::list<Certificate> &certificates);

        std::list<Certificate> certificates() const;

        bool isEmpty() const;

        void clear();

        enum class VerifyOption : uint32_t
        {
            None = 0,
            CheckCertificateExpiry = (1 << 0),
        };

        LIBRALF_DECLARE_ENUM_FLAGS(VerifyOptions, VerifyOption)

        // -------------------------------------------------------------------------
        /*!
            Verifies the \a certificate against the root certificates in this bundle.
            If the bundle contains no certificates then the verification will fail.
            \a untrustedCerts is an optional list of certificates that are not trusted
            but can for creating a verified chain of trust.

            \a options is a set of options that can be used to modify the verification
            process.

            If the verification fails then \c false is returned and if \a error was
            not null it will be set with the reason why.

         */
        Result<> verifyCertificate(const Certificate &certificate, const std::list<Certificate> &untrustedCerts,
                                   VerifyOptions options = VerifyOption::None) const;

        // -------------------------------------------------------------------------
        /*!
            \overload

            Overload of verifyCertificate that does not take a list of untrusted.

         */
        inline Result<> verifyCertificate(const Certificate &certificate, VerifyOptions options = VerifyOption::None) const
        {
            return verifyCertificate(certificate, {}, options);
        }

    private:
        friend class Package;

        std::unique_ptr<IVerificationBundleImpl> m_impl;
    };

    LIBRALF_DECLARE_ENUM_FLAGS_OPERATORS(VerificationBundle::VerifyOptions)

} // namespace LIBRALF_NS
