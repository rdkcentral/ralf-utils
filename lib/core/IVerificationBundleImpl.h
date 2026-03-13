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

#include "EnumFlags.h"
#include "Error.h"
#include "LibRalf.h"
#include "VerificationBundle.h"

#include <cinttypes>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <vector>

namespace LIBRALF_NS
{
    class ICertificateImpl;

    class IVerificationBundleImpl
    {
    public:
        virtual ~IVerificationBundleImpl() = default;

        virtual std::unique_ptr<IVerificationBundleImpl> clone() = 0;

        virtual void addCertificate(const std::shared_ptr<ICertificateImpl> &certificate) = 0;
        virtual void clearCertificates() = 0;

        virtual std::list<std::shared_ptr<ICertificateImpl>> certificates() const = 0;

        virtual bool isEmpty() const = 0;

        virtual void clear() = 0;

        virtual bool verifyPkcs7(const void *_Nonnull pkcs7Blob, size_t pkcs7BlobSize,
                                 VerificationBundle::VerifyOptions options, std::vector<uint8_t> *_Nullable contents,
                                 Error *_Nullable error) const = 0;

        virtual bool verifyCertificate(const std::shared_ptr<ICertificateImpl> &certificate,
                                       const std::vector<std::shared_ptr<ICertificateImpl>> &untrustedCerts,
                                       VerificationBundle::VerifyOptions options, Error *_Nullable error) const = 0;
    };

} // namespace LIBRALF_NS
