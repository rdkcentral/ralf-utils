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

#include "Error.h"
#include "LibRalf.h"
#include "Result.h"

#include <chrono>
#include <filesystem>
#include <list>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace LIBRALF_NS
{
    class ICertificateImpl;

    // -------------------------------------------------------------------------
    /*!
        \class Certificate
        \brief Helper class for working with X509 certificates.

        Provides methods for reading and writing certificates in PEM and DER
        format.  Also provides methods for extracting information from the
        certificates.

        The class is designed to be used with the VerificationBundle class.

     */
    class LIBRALF_EXPORT Certificate
    {
    public:
        enum EncodingFormat : unsigned
        {
            PEM = 0,
            DER
        };

    public:
        // -------------------------------------------------------------------------
        /*!
            Loads a certificate from the file at \a filePath.  The \a format must
            match the file being read.

            \a error is optional, if non-null it will be set with any error that
            occurs during the load.

            On failure a null certificate is returned.
         */
        static Result<Certificate> loadFromFile(const std::filesystem::path &filePath,
                                                EncodingFormat format = EncodingFormat::PEM);

        // -------------------------------------------------------------------------
        /*!
            Loads a PEM certificate from string \a str.

            \a error is optional, if non-null it will be set with any error that
            occurs during the load.

            On failure a null certificate is returned.
         */
        static Result<Certificate> loadFromString(std::string_view str);

        // -------------------------------------------------------------------------
        /*!
            Loads either a PEM or DER certificate from binary buffer.  You'd typically
            use this for loading DER formatted certificates as they are binary.

            \a error is optional, if non-null it will be set with any error that
            occurs during the load.

            On failure a null certificate is returned.

            \sa loadFromMemory
         */
        static Result<Certificate> loadFromVector(const std::vector<uint8_t> &data,
                                                  EncodingFormat format = EncodingFormat::DER);

        // -------------------------------------------------------------------------
        /*!
            Loads either a PEM or DER certificate from memory buffer.

            \a error is optional, if non-null it will be set with any error that
            occurs during the load.

            On failure a null certificate is returned.
         */
        static Result<Certificate> loadFromMemory(const void *data, size_t length,
                                                  EncodingFormat format = EncodingFormat::DER);

        // -------------------------------------------------------------------------
        /*!
            Loads a list of X509 certificates in PEM format from the file. If the
            file contains non-X509 certificates they will be skipped.

            \a error is optional, if non-null it will be set with any error that
            occurs during the load.

            On failure an empty list is returned.
         */
        static Result<std::list<Certificate>> loadFromFileMultiCerts(const std::filesystem::path &filePath);

        // -------------------------------------------------------------------------
        /*!
            Loads a list of X509 certificates in PEM format from the string.

            \a error is optional, if non-null it will be set with any error that
            occurs during the load.

            On failure an empty list is returned.
         */
        static Result<std::list<Certificate>> loadFromStringMultiCerts(std::string_view str);

    public:
        // -------------------------------------------------------------------------
        /*!
            Constructs a null certificate.
         */
        Certificate();

        // -------------------------------------------------------------------------
        /*!
            Constructs an identical copy of the \a other certificate.
         */
        Certificate(const Certificate &other);

        // -------------------------------------------------------------------------
        /*!
            Move constructs a new certificate from \a other.

            \a other is left in a null state.
         */
        Certificate(Certificate &&other) noexcept;

        ~Certificate();

        // -------------------------------------------------------------------------
        /*!
            Sets this certificate to be an identical copy of the \a other certificate.
         */
        Certificate &operator=(const Certificate &other);

        // -------------------------------------------------------------------------
        /*!
            Moves \a other certificate into this certificate. \a other is left in a
            null state.
         */
        Certificate &operator=(Certificate &&other) noexcept;

        // -------------------------------------------------------------------------
        /*!
            Returns \c true if this certificate is not equal to the \a other certificate.
         */
        bool operator!=(const Certificate &other) const;

        // -------------------------------------------------------------------------
        /*!
            Returns \c false if this certificate is not equal to the \a other certificate.
         */
        bool operator==(const Certificate &other) const;

        // -------------------------------------------------------------------------
        /*!
            Returns \c true if certificate object doesn't contains a valid X509
            certificate.

            \sa isValid.
         */
        bool isNull() const;

        // -------------------------------------------------------------------------
        /*!
            Returns \c true if certificate object contains a valid X509 certificate.
            This is the opposite of isNull().

            \sa isNull.
         */
        bool isValid() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the subject string of the certificate if it has one.

            Returns an empty string if the certificate is not valid.
         */
        std::string subject() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the name of the issuer of the certificate if it has one.

            Returns an empty string if the certificate is not valid.
         */
        std::string issuer() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the common name of the certificate if it has one.

            Returns an empty string if the certificate is not valid.
        */
        std::string commonName() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the 'notBefore' time of the certificate as a chrono time point.

         */
        std::chrono::system_clock::time_point notBefore() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the 'notAfter' time of the certificate as a chrono time point.

        */
        std::chrono::system_clock::time_point notAfter() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the certificate as a string in a similar format to the openssl
            command line tool.

        */
        std::string toString() const;

    protected:
        friend class DigitalSignature;
        friend class VerificationBundle;
        friend std::ostream &operator<<(std::ostream &, const Certificate &);

        explicit Certificate(std::shared_ptr<ICertificateImpl> impl);

    private:
        std::shared_ptr<ICertificateImpl> m_impl;
    };

    LIBRALF_EXPORT std::ostream &operator<<(std::ostream &s, const Certificate &cert);

} // namespace LIBRALF_NS