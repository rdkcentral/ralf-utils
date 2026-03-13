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

#include "Error.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

namespace
{
    class LibRalfErrCategory : public std::error_category
    {
    public:
        ~LibRalfErrCategory() override = default;

        const char *name() const noexcept override;
        std::string message(int ev) const override;
    };

    const char *LibRalfErrCategory::name() const noexcept
    {
        return "libralf";
    }

    std::string LibRalfErrCategory::message(int ev) const
    {
        switch (static_cast<ErrorCode>(ev))
        {
            case ErrorCode::None:
                return "No error";

            case ErrorCode::InvalidPackage:
                return "Invalid package";
            case ErrorCode::InvalidArgument:
                return "Invalid argument";
            case ErrorCode::InternalError:
                return "Internal error";
            case ErrorCode::NotSupported:
                return "Not supported";
            case ErrorCode::CertificateError:
                return "Invalid certificate";
            case ErrorCode::GenericCryptoError:
                return "Generic error in crypto library";
            case ErrorCode::PackageFormatNotSupported:
                return "Package format not supported";
            case ErrorCode::ArchiveError:
                return "Archive error";
            case ErrorCode::PackageSignatureMissing:
                return "Package signature missing";
            case ErrorCode::PackageSignatureInvalid:
                return "Package signature invalid";
            case ErrorCode::PackageContentsInvalid:
                return "Package contents invalid";
            case ErrorCode::PackageFileTooLarge:
                return "Package file too large";
            case ErrorCode::PackageInvalidEntry:
                return "Package entry invalid";
            case ErrorCode::XmlParseError:
                return "XML parse error";
            case ErrorCode::XmlSchemaError:
                return "XML schema error";
            case ErrorCode::GenericXmlError:
                return "Generic XML error";
            case ErrorCode::CertificateExpired:
                return "Certificate expired";
            case ErrorCode::CertificateRevoked:
                return "Certificate revoked";
            case ErrorCode::InvalidMetaData:
                return "Invalid metadata";
            case ErrorCode::FileExists:
                return "File exists";
            case ErrorCode::DmVerityError:
                return "DM Verity error";
            case ErrorCode::ErofsError:
                return "EROFS error";
            case ErrorCode::VersionNumberTooManyFields:
                return "Version number has too many fields";
            case ErrorCode::VersionNumberInvalidCharacter:
                return "Version number has invalid character";
            case ErrorCode::VersionNumberTooLong:
                return "Version number is too long";
            case ErrorCode::VersionNumberEmpty:
                return "Version number is empty";
            case ErrorCode::VersionConstraintInvalid:
                return "Version constraint is invalid";
            case ErrorCode::PackageMountInvalid:
                return "Package mount is invalid";
            case ErrorCode::FileNotFound:
                return "File not found";
            case ErrorCode::PackageExtractLimitExceeded:
                return "Package extract limit exceeded";
        }

        return std::string("Undefined error: ") + std::to_string(ev);
    }

    const LibRalfErrCategory theLibRalfErrCategory{};

} // namespace

std::error_code LIBRALF_NS::make_error_code(LIBRALF_NS::ErrorCode e)
{
    return { static_cast<int>(e), theLibRalfErrCategory };
}

Error::Error() noexcept
    : m_code(ErrorCode::None)
{
}

Error::Error(std::error_code ec)
    : m_code(ec)
    , m_what(ec.message())
{
}

Error::Error(std::error_code ec, std::string what)
    : m_code(ec)
    , m_what(std::move(what))
{
}

Error::Error(std::error_code ec, const char *what)
    : m_code(ec)
    , m_what(what)
{
}

Error::Error(const Error &other) // NOLINT(modernize-use-equals-default)
    : m_code(other.m_code)
    , m_what(other.m_what)
{
}

Error::Error(Error &&other) noexcept // NOLINT(modernize-use-equals-default)
    : m_code(other.m_code)
    , m_what(std::move(other.m_what))
{
    other.m_code.clear();
}

Error::~Error() {} // NOLINT(modernize-use-equals-default)

Error &Error::operator=(std::error_code ec) noexcept
{
    m_code = ec;
    m_what = ec.message();
    return *this;
}

Error &Error::operator=(const Error &other) // NOLINT(modernize-use-equals-default)
{
    m_code = other.m_code;
    m_what = other.m_what;
    return *this;
}

Error &Error::operator=(Error &&other) noexcept // NOLINT(modernize-use-equals-default)
{
    m_code = other.m_code;
    other.m_code.clear();
    m_what = std::move(other.m_what);
    return *this;
}

void Error::clear() noexcept
{
    m_code.clear();
    m_what.clear();
}

void Error::assign(std::error_code ec)
{
    m_code = ec;
    m_what = ec.message();
}

void Error::assign(std::error_code ec, std::string what)
{
    m_code = ec;
    m_what.assign(std::move(what));
}

void Error::assign(std::error_code ec, const char *what)
{
    m_code = ec;
    m_what = what;
}

Error Error::format(std::error_code ec, const char *fmt, va_list args)
{
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);

    return { ec, buf };
}

Error Error::format(std::error_code ec, const char *fmt, ...)
{
    char buf[512];

    std::va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    return { ec, buf };
}
