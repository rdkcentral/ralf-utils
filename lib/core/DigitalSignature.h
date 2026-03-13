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
#include "Compatibility.h"
#include "Error.h"

#include <memory>
#include <string>
#include <vector>

namespace LIBRALF_NS
{
    class IDigitalSignatureImpl;

    class DigitalSignature
    {
    public:
        // -------------------------------------------------------------------------
        /*!
            The possible hashing algorithms that can be used to verify or sign the
            data.
        */
        enum class Algorithm
        {
            Null = 0,

            Sha1 = 200,
            Sha224,
            Sha256,
            Sha384,
            Sha512,
        };

        DigitalSignature() = delete;
        DigitalSignature(const DigitalSignature &) = delete;
        DigitalSignature &operator=(const DigitalSignature &) = delete;

        // -------------------------------------------------------------------------
        /*!
            Constructs the builder which will use the given \a algorithm to create
            the digest.
        */
        explicit DigitalSignature(Algorithm algorithm);

        // -------------------------------------------------------------------------
        /*!
            Move constructs a new builder from \a other.  \a other is reset to it's
            initial state.
        */
        DigitalSignature(DigitalSignature &&other) noexcept;

        // -------------------------------------------------------------------------
        /*!
            Move assigns \a other to this instance.  \a other is reset to it's
            initial state.
        */
        DigitalSignature &operator=(DigitalSignature &&other) noexcept;

        // -------------------------------------------------------------------------
        /*!
            Destructor.
        */
        ~DigitalSignature();

        // -------------------------------------------------------------------------
        /*!
            Updates the digest with the given \a data of \a length bytes.
        */
        void update(const void *_Nullable data, size_t length);

        // -------------------------------------------------------------------------
        /*!
            Updates the digest with the given \a data.
        */
        inline void update(const std::vector<uint8_t> &data) { update(data.data(), data.size()); }

        // -------------------------------------------------------------------------
        /*!
            Updates the digest with the given \a data.
        */
        inline void update(const std::string &data) { update(data.data(), data.size()); }

        // -------------------------------------------------------------------------
        /*!
            Resets the digest to it's initial state.
        */
        void reset();

        // -------------------------------------------------------------------------
        /*!
            Verifies the data added to the object against the supplied \a signature
            data and the public key stored in the supplied \a certificate.

            Returns \c true if the verification was successful, \c false otherwise.
            If \a error is not nullptr and the verification failed then it is populated
            with the error details.
        */
        bool verify(const Certificate &certificate, const void *_Nullable signature, size_t signatureLength,
                    Error *_Nullable error = nullptr) const;

        inline bool verify(const Certificate &certificate, const std::vector<uint8_t> &signature,
                           Error *_Nullable error = nullptr) const
        {
            return verify(certificate, signature.data(), signature.size(), error);
        }

    private:
        std::unique_ptr<IDigitalSignatureImpl> m_impl;
    };

} // namespace LIBRALF_NS
