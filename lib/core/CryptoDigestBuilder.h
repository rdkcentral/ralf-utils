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

#include "Compatibility.h"
#include "LibRalf.h"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace LIBRALF_NS
{
    class ICryptoDigestBuilderImpl;

    class CryptoDigestBuilder
    {
    public:
        // -------------------------------------------------------------------------
        /*!
            The possible algorithms that can be used to create a digest.
        */
        enum class Algorithm
        {
            Null = 0,

            Md5 = 100,

            Sha1 = 200,
            Sha256,
            Sha384,
            Sha512,
        };

        CryptoDigestBuilder() = delete;
        CryptoDigestBuilder(const CryptoDigestBuilder &) = delete;
        CryptoDigestBuilder &operator=(const CryptoDigestBuilder &) = delete;

        // -------------------------------------------------------------------------
        /*!
            Constructs the builder which will use the given \a algorithm to create
            the digest.
        */
        explicit CryptoDigestBuilder(Algorithm algorithm);

        // -------------------------------------------------------------------------
        /*!
            Move constructs a new builder from \a other.  \a other is reset to it's
            initial state.
        */
        CryptoDigestBuilder(CryptoDigestBuilder &&other) noexcept;

        // -------------------------------------------------------------------------
        /*!
            Move assigns \a other to this instance.  \a other is reset to it's
            initial state.
        */
        CryptoDigestBuilder &operator=(CryptoDigestBuilder &&other) noexcept;

        // -------------------------------------------------------------------------
        /*!
            Destructor.
        */
        ~CryptoDigestBuilder();

        // -------------------------------------------------------------------------
        /*!
            Updates the digest with the given \a data of \a length bytes.

            As a special case, if \a data is \c nullptr then the digest is updated
            with \a length number of zero bytes. For example the following are
            equivalent:
            \code
                builder.update(nullptr, 10);
            \endcode
            \code
                uint8_t zero[10] = { 0 };
                builder.update(zero, 10);
            \endcode
        */
        void update(const void *_Nullable data, size_t length);

        // -------------------------------------------------------------------------
        /*!
            Updates the digest with the given \a data.
        */
        inline void update(const std::vector<uint8_t> &data) { update(data.data(), data.size()); }

        // -------------------------------------------------------------------------
        /*!
            Resets the digest to it's initial state.
        */
        void reset();

        // -------------------------------------------------------------------------
        /*!
            Returns the finalised digest.  After this call the builder is effectively
            reset to it's initial state.
        */
        std::vector<uint8_t> finalise() const;

    public:
        // -------------------------------------------------------------------------
        /*!
            \static

            Helper function to run a digest over the given \a data of \a length using
            the given \a algorithm.
         */
        static std::vector<uint8_t> digest(Algorithm algorithm, const void *_Nullable data, size_t length);

        // -------------------------------------------------------------------------
        /*!
            \static

            Helper function to run a digest over the given \a data using the given
            \a algorithm.
         */
        static inline std::vector<uint8_t> digest(Algorithm algorithm, const std::vector<uint8_t> &data)
        {
            return digest(algorithm, data.data(), data.size());
        }

        // -------------------------------------------------------------------------
        /*!
            \static

            Helper function to run a digest over the given \a data string using the
            given \a algorithm.
         */
        static inline std::vector<uint8_t> digest(Algorithm algorithm, const std::string &data)
        {
            return digest(algorithm, data.data(), data.size());
        }

        // -------------------------------------------------------------------------
        /*!
            \static

            Optimised helper function to calculate a SHA256 digest over the given
            \a data of \a length with an optional \a salt of \a saltSize bytes.

            This is designed to be used by the dm-verity code which is performance
            critical, and performs lots of small 4096 bytes hashes.

         */
        static std::array<uint8_t, 32> sha256Digest(const void *_Nullable data, size_t length,
                                                    const void *_Nullable salt = nullptr, size_t saltSize = 0);

    private:
        std::unique_ptr<ICryptoDigestBuilderImpl> m_impl;
    };

} // namespace LIBRALF_NS
