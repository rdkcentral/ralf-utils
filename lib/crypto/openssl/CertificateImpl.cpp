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

#include "CertificateImpl.h"
#include "CryptoUtils.h"
#include "core/LogMacros.h"

#include <openssl/pem.h>
#include <openssl/x509.h>

#include <fcntl.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Helper utility to populate the \a error object with the given error code and
    message.

 */
static void reportError(Error *_Nullable error, const std::error_code &errorCode, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

static void reportError(Error *_Nullable error, const std::error_code &errorCode, const char *format, ...)
{
    if (!error)
        return;

    std::va_list args;
    va_start(args, format);
    *error = Error::format(errorCode, format, args);
    va_end(args);
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Helper utility to parse a certificate from a BIO object.

 */
static X509UniquePtr parseCert(const BIOUniquePtr &bio, Certificate::EncodingFormat format, Error *_Nullable error)
{
    ERR_clear_error();

    X509 *x509 = nullptr;
    if (format == Certificate::EncodingFormat::PEM)
    {
        x509 = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
    }
    else if (format == Certificate::EncodingFormat::DER)
    {
        x509 = d2i_X509_bio(bio.get(), nullptr);
    }
    else
    {
        reportError(error, ErrorCode::InvalidArgument, "Unknown encoding type");
        return nullptr;
    }

    if (!x509)
    {
        reportError(error, ErrorCode::CertificateError, "Failed to parse %s certificate",
                    (format == Certificate::EncodingFormat::PEM) ? "PEM" : "DER");
    }

    return { x509, X509_free };
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Helper utility to parse a certificate from a BIO object.

*/
static std::vector<X509UniquePtr> parseMultipleCert(const BIOUniquePtr &bio, Error *_Nullable error)
{
    (void)error;

    std::vector<X509UniquePtr> certs;

    X509 *cert = nullptr;
    while (!BIO_eof(bio.get()) && ((cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr)) != nullptr))
    {
        certs.emplace_back(cert, X509_free);
    }

    return certs;
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Helper utility to convert an OpenSSL X509_NAME object to a string in RFC2253
    format.

 */
static std::string x509NameToString(X509_NAME *name)
{
    ERR_clear_error();

    if (!name)
        return {};

    auto bio = BIOUniquePtr(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio)
        return {};

    if (X509_NAME_print_ex(bio.get(), name, 0, XN_FLAG_RFC2253) == 0)
        return {};

    BUF_MEM *p = nullptr;
    BIO_get_mem_ptr(bio.get(), &p);
    if (!p || !p->data || !p->length)
        return {};

    return { p->data, p->length };
}

std::shared_ptr<CertificateImpl> CertificateImpl::loadFromFile(const std::filesystem::path &filePath,
                                                               Certificate::EncodingFormat format, Error *_Nullable error)
{
    if (error)
        error->clear();

    ERR_clear_error();

    int fd = open(filePath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        const std::error_code errCode(errno, std::system_category());
        reportError(error, errCode, "Failed to open '%s' (%d - %s)", filePath.c_str(), errCode.value(),
                    errCode.message().c_str());
        return nullptr;
    }

    BIOUniquePtr bio(BIO_new_fd(fd, BIO_CLOSE), BIO_vfree);
    if (!bio)
    {
        reportError(error, ErrorCode::GenericCryptoError, "Failed to create bio for '%s'", filePath.c_str());
        close(fd);
        return nullptr;
    }

    X509UniquePtr x509 = parseCert(bio, format, error);
    if (!x509)
    {
        // error already logged
        return nullptr;
    }

    return std::make_shared<CertificateImpl>(x509.release());
}

std::shared_ptr<CertificateImpl> CertificateImpl::loadFromMemory(const void *_Nonnull data, size_t length,
                                                                 Certificate::EncodingFormat format,
                                                                 Error *_Nullable error)
{
    if (error)
        error->clear();

    ERR_clear_error();

    BIOUniquePtr bio(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio)
    {
        reportError(error, std::error_code(ENOMEM, std::system_category()), "Cannot allocate memory bio");
        return nullptr;
    }

    if (BIO_write(bio.get(), data, static_cast<int>(length)) != static_cast<int>(length))
    {
        reportError(error, ErrorCode::GenericCryptoError, "Failed to populate bio with data");
        return nullptr;
    }

    X509UniquePtr x509 = parseCert(bio, format, error);
    if (!x509)
    {
        // error already logged
        return nullptr;
    }

    return std::make_shared<CertificateImpl>(x509.release());
}

std::list<std::shared_ptr<CertificateImpl>> CertificateImpl::loadFromFileMultiCerts(const std::filesystem::path &filePath,
                                                                                    Error *_Nullable error)
{
    if (error)
        error->clear();

    ERR_clear_error();

    int fd = open(filePath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        const std::error_code errCode(errno, std::system_category());
        reportError(error, errCode, "Failed to open '%s' (%d - %s)", filePath.c_str(), errCode.value(),
                    errCode.message().c_str());
        return {};
    }

    BIOUniquePtr bio(BIO_new_fd(fd, BIO_CLOSE), BIO_vfree);
    if (!bio)
    {
        reportError(error, ErrorCode::GenericCryptoError, "Failed to create bio for '%s'", filePath.c_str());
        close(fd);
        return {};
    }

    auto x509Certs = parseMultipleCert(bio, error);

    std::list<std::shared_ptr<CertificateImpl>> certs;
    for (X509UniquePtr &cert : x509Certs)
    {
        certs.emplace_back(std::make_shared<CertificateImpl>(cert.release()));
    }

    return certs;
}

std::list<std::shared_ptr<CertificateImpl>> CertificateImpl::loadFromMemoryMultiCerts(const void *data, size_t length,
                                                                                      Error *_Nullable error)
{
    if (error)
        error->clear();

    ERR_clear_error();

    BIOUniquePtr bio(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio)
    {
        reportError(error, std::error_code(ENOMEM, std::system_category()), "Cannot allocate memory bio");
        return {};
    }

    if (BIO_write(bio.get(), data, static_cast<int>(length)) != static_cast<int>(length))
    {
        reportError(error, ErrorCode::GenericCryptoError, "Failed to populate bio with data");
        return {};
    }

    auto x509Certs = parseMultipleCert(bio, error);

    std::list<std::shared_ptr<CertificateImpl>> certs;
    for (X509UniquePtr &cert : x509Certs)
    {
        certs.emplace_back(std::make_shared<CertificateImpl>(cert.release()));
    }

    return certs;
}

CertificateImpl::CertificateImpl(X509 *_Nonnull cert)
    : m_x509Cert(cert)
{
}

CertificateImpl::~CertificateImpl()
{
    if (m_x509Cert)
        X509_free(m_x509Cert);
}

std::string CertificateImpl::subject() const
{
    if (!m_x509Cert)
        return {};
    else
        return x509NameToString(X509_get_subject_name(m_x509Cert));
}

std::string CertificateImpl::issuer() const
{
    if (!m_x509Cert)
        return {};
    else
        return x509NameToString(X509_get_issuer_name(m_x509Cert));
}

std::string CertificateImpl::commonName() const
{
    if (!m_x509Cert)
        return {};

    ERR_clear_error();

    X509_NAME *subjectName = X509_get_subject_name(m_x509Cert);
    if (!subjectName)
    {
        logWarning("Failed to find subject in certificate");
        return {};
    }

    int index = X509_NAME_get_index_by_NID(subjectName, NID_commonName, -1);
    if (index == -1)
    {
        logWarning("Failed to find common name in certificate");
        return {};
    }

    X509_NAME_ENTRY *entry = X509_NAME_get_entry(subjectName, index);
    ASN1_STRING *asn1Str = X509_NAME_ENTRY_get_data(entry);

    unsigned char *utf8 = nullptr;
    int length = ASN1_STRING_to_UTF8(&utf8, asn1Str);
    if ((length < 0) || !utf8)
    {
        logError("Failed to convert common name to UTF8");
        return {};
    }

    std::string cn(reinterpret_cast<char *>(utf8), length);

    OPENSSL_free(utf8);

    return cn;
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Helper utility to convert an OpenSSL ASN1_TIME object to system time point.

 */
static std::chrono::system_clock::time_point asn1TimeToTimePoint(const ASN1_TIME *asn1Time)
{
    // in an ideal world we'd convert the time to a std::tm structure and then convert that to a time_t, however on
    // some 32-bit systems we'll hit a problem if the certificate expires after 2038 due to time_t being 32-bit.
    // So instead we calculate the ASN time diff with current time and then add that to the current chrono time.

    const auto now = std::chrono::system_clock::now();
    auto nowAsn1time = ASN1_TIMEUniquePtr(ASN1_TIME_new(), ASN1_TIME_free);
    if (!nowAsn1time)
    {
        logError("Failed to create ASN1 time");
        return {};
    }

    if (!ASN1_TIME_set(nowAsn1time.get(), std::chrono::system_clock::to_time_t(now)))
    {
        logError("Failed to set temporary ASN1 time to now");
        return {};
    }

    // this doesn't take leap seconds into account, but that's probably not an issue as cert times are usually
    // over a long period of time
    int day, sec;
    if (ASN1_TIME_diff(&day, &sec, nowAsn1time.get(), asn1Time) == 0)
    {
        logError("Failed to calculate time difference between now and ASN1 time");
        return {};
    }

    return std::chrono::time_point_cast<std::chrono::seconds>(now) +
           std::chrono::seconds((static_cast<int64_t>(day) * 24 * 60 * 60) + static_cast<int64_t>(sec));
}

std::chrono::system_clock::time_point CertificateImpl::notBefore() const
{
    if (!m_x509Cert)
        return {};

    ERR_clear_error();

    const ASN1_TIME *asn1Time = X509_get0_notBefore(m_x509Cert);
    if (!asn1Time)
    {
        logError("Unable to retrieve certificate notBefore date");
        return {};
    }

    return asn1TimeToTimePoint(asn1Time);
}

std::chrono::system_clock::time_point CertificateImpl::notAfter() const
{
    if (!m_x509Cert)
        return {};

    ERR_clear_error();

    const ASN1_TIME *asn1Time = X509_get0_notAfter(m_x509Cert);
    if (!asn1Time)
    {
        logError("Unable to retrieve certificate notAfter date");
        return {};
    }

    return asn1TimeToTimePoint(asn1Time);
}

X509 *CertificateImpl::x509Cert() const
{
    return m_x509Cert;
}

bool CertificateImpl::isSame(const ICertificateImpl *other) const
{
    if (!m_x509Cert)
        return false;

    auto otherImpl = dynamic_cast<const CertificateImpl *>(other);
    if (!otherImpl || !otherImpl->m_x509Cert)
        return false;

    return X509_cmp(m_x509Cert, otherImpl->m_x509Cert) == 0;
}

std::string CertificateImpl::toPem() const
{
    if (!m_x509Cert)
        return {};

    ERR_clear_error();

    auto bio = BIOUniquePtr(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio)
        return {};

    if (PEM_write_bio_X509(bio.get(), m_x509Cert) == 0)
        return {};

    BUF_MEM *bufferPtr;
    BIO_get_mem_ptr(bio.get(), &bufferPtr);
    if (!bufferPtr || !bufferPtr->data || !bufferPtr->length)
        return {};

    return { bufferPtr->data, bufferPtr->length };
}

std::vector<uint8_t> CertificateImpl::toDer() const
{
    if (!m_x509Cert)
        return {};

    ERR_clear_error();

    auto bio = BIOUniquePtr(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio)
        return {};

    if (i2d_X509_bio(bio.get(), m_x509Cert) == 0)
        return {};

    BUF_MEM *bufferPtr;
    BIO_get_mem_ptr(bio.get(), &bufferPtr);
    if (!bufferPtr || !bufferPtr->data || !bufferPtr->length)
        return {};

    return { bufferPtr->data, bufferPtr->data + bufferPtr->length };
}

std::string CertificateImpl::toString() const
{
    if (!m_x509Cert)
        return "<null>";

    auto bio = BIOUniquePtr(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio)
        return {};

    X509_print(bio.get(), m_x509Cert);

    BUF_MEM *bufferPtr;
    BIO_get_mem_ptr(bio.get(), &bufferPtr);
    if (!bufferPtr || !bufferPtr->data || !bufferPtr->length)
        return {};

    return { bufferPtr->data, bufferPtr->length };
}
