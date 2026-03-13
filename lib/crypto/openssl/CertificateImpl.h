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
#include "Error.h"
#include "core/Compatibility.h"
#include "core/ICertificateImpl.h"

#include <memory>

struct x509_st;
typedef struct x509_st X509;

class CertificateImpl final : public LIBRALF_NS::ICertificateImpl
{
public:
    static std::shared_ptr<CertificateImpl> loadFromFile(const std::filesystem::path &filePath,
                                                         LIBRALF_NS::Certificate::EncodingFormat format,
                                                         LIBRALF_NS::Error *_Nullable error);

    static std::shared_ptr<CertificateImpl> loadFromMemory(const void *_Nonnull data, size_t length,
                                                           LIBRALF_NS::Certificate::EncodingFormat format,
                                                           LIBRALF_NS::Error *_Nullable error);

    static std::list<std::shared_ptr<CertificateImpl>> loadFromFileMultiCerts(const std::filesystem::path &filePath,
                                                                              LIBRALF_NS::Error *_Nullable error);

    static std::list<std::shared_ptr<CertificateImpl>> loadFromMemoryMultiCerts(const void *_Nonnull data, size_t length,
                                                                                LIBRALF_NS::Error *_Nullable error);

public:
    explicit CertificateImpl(X509 *_Nonnull cert);
    ~CertificateImpl() final;

    std::string subject() const override;
    std::string issuer() const override;
    std::string commonName() const override;

    std::chrono::system_clock::time_point notBefore() const override;
    std::chrono::system_clock::time_point notAfter() const override;

    std::string toString() const override;

    bool isSame(const ICertificateImpl *_Nullable other) const override;

    std::string toPem() const override;
    std::vector<uint8_t> toDer() const override;

    X509 *_Nonnull x509Cert() const;

private:
    X509 *_Nonnull m_x509Cert;
};
