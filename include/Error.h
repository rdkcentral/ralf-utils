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

#include "LibRalf.h"

#include <cstdarg>
#include <cstdint>
#include <string>
#include <system_error>

namespace LIBRALF_NS
{

    // -----------------------------------------------------------------------------
    /*!
        \enum ErrorCode
        \brief Potential error codes that can be returned by the library.

        This object is modelled on the std::error_code object, it represents an
        error that occurred while processing packages or credentials.

    */
    enum class ErrorCode : uint32_t
    {
        None = 0,
        NoError = 0,
        InvalidPackage = 1,
        InvalidArgument = 2,
        InternalError = 3,
        NotSupported = 4,
        CertificateError = 5,
        GenericCryptoError = 6,
        PackageFormatNotSupported = 7,
        ArchiveError = 8,
        PackageSignatureMissing = 9,
        PackageSignatureInvalid = 10,
        PackageContentsInvalid = 11,
        PackageFileTooLarge = 12,
        PackageInvalidEntry = 13,
        XmlParseError = 14,
        XmlSchemaError = 15,
        GenericXmlError = 16,
        CertificateExpired = 17,
        CertificateRevoked = 18,
        InvalidMetaData = 19,
        FileExists = 20,
        DmVerityError = 21,
        ErofsError = 22,
        VersionNumberTooManyFields = 23,
        VersionNumberInvalidCharacter = 24,
        VersionNumberTooLong = 25,
        VersionNumberEmpty = 26,
        VersionConstraintInvalid = 27,
        PackageMountInvalid = 28,
        FileNotFound = 29,
        PackageExtractLimitExceeded = 30,
    };

    LIBRALF_EXPORT std::error_code make_error_code(ErrorCode e);

    // -----------------------------------------------------------------------------
    /*!
        \class Error
        \brief Object use to report errors.

        This object is modelled on the std::system_error object, it represents an
        error that occurred while processing packages or credentials.

     */
    class LIBRALF_EXPORT Error
    {
    public:
        Error() noexcept;
        Error(std::error_code ec, std::string what);
        Error(std::error_code ec, const char *what);
        explicit Error(std::error_code ec);
        Error(const Error &other);
        Error(Error &&other) noexcept;
        ~Error();

        const std::error_code &code() const noexcept { return m_code; }

        Error &operator=(std::error_code ec) noexcept;
        Error &operator=(const Error &other);
        Error &operator=(Error &&other) noexcept;

        void clear() noexcept;
        void assign(std::error_code ec, std::string what);
        void assign(std::error_code ec, const char *what);
        void assign(std::error_code ec);

        const char *what() const noexcept { return m_what.c_str(); }

        explicit operator bool() const noexcept { return m_code.operator bool(); }

        static Error format(std::error_code ec, const char *fmt, va_list args) __attribute__((format(printf, 2, 0)));
        static Error format(std::error_code ec, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

    private:
        std::error_code m_code;
        std::string m_what;
    };

    inline bool operator==(const Error &lhs, const Error &rhs)
    {
        return (lhs.code() == rhs.code()) && (lhs.what() == rhs.what());
    }

    inline bool operator!=(const Error &lhs, const Error &rhs)
    {
        return !(lhs == rhs);
    }

} // namespace LIBRALF_NS

namespace std
{
    template <>
    struct is_error_code_enum<LIBRALF_NS::ErrorCode> : true_type
    {
    };
} // namespace std
