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
#include "Result.h"

#include <string>
#include <string_view>

namespace LIBRALF_NS
{

    // -----------------------------------------------------------------------------
    /*!
        \class VersionNumber
        \brief Object stores a strict semantic version number.

        This class is used to store a version number in the form of up to 4 parts;
        major, minor, patch and build.  It provides methods to compare versions
        as well as convert to and from a string representation.

     */
    class LIBRALF_EXPORT VersionNumber
    {
    public:
        VersionNumber();
        explicit VersionNumber(unsigned v0);
        VersionNumber(unsigned v0, unsigned v1);
        VersionNumber(unsigned v0, unsigned v1, unsigned v2);
        VersionNumber(unsigned v0, unsigned v1, unsigned v2, unsigned v3);

        VersionNumber(const VersionNumber &other) = default;
        VersionNumber(VersionNumber &&other) noexcept = default;

        VersionNumber &operator=(const VersionNumber &other) = default;
        VersionNumber &operator=(VersionNumber &&other) noexcept = default;

        size_t fieldCount() const;
        unsigned fieldValue(size_t indexField) const;

        // -------------------------------------------------------------------------
        /*!
            \static

            Converts a string representation of a version number to a VersionNumber
            object.  The string must be in the form of "major.minor.patch.build"
            where major, minor, patch and build are all unsigned integers. The
            string must contain a major version number, but the rest are optional.

            In addition leading zeros are not allowed, so "01.2.3" is invalid.

            If the string is invalid, then an error Result is returned.
         */
        static Result<VersionNumber> fromString(std::string_view str);

        // -------------------------------------------------------------------------
        /*!
            Converts the version number to a string representation.

         */
        std::string toString() const;

        // -------------------------------------------------------------------------
        /*!
            Compares two version numbers.  Returns \c -1 if \a lhs < \a rhs,
            \c 0 if \a lhs == \a rhs and \c 1 if \a lhs > \a rhs.

            \code
                VersionNumber ver1(1, 2, 3);
                VersionNumber ver2(1, 2, 4);
                assert(VersionNumber::compare(ver1, ver2) == -1);
            \endcode
         */
        static int compare(const VersionNumber &lhs, const VersionNumber &rhs);

        // -------------------------------------------------------------------------
        /*!
            Preincrement the version number by one.  This is a simple implementation
            that increments the last field of the version number.  If the last field
            is at its maximum value, it rolls over to zero and increments the next
            field, and so on.

            \code
                VersionNumber ver1(1, 2, 3);
                assert(++ver1 == VersionNumber(1, 2, 4));
            \endcode
         */
        VersionNumber &operator++();

        // -------------------------------------------------------------------------
        /*!
            Postincrement the version number by one.  This is a simple implementation
            that increments the last field of the version number.  If the last field
            is at its maximum value, it rolls over to zero and increments the next
            field, and so on.

            \code
                VersionNumber ver1(1, 2, 3);
                assert(++ver1 == VersionNumber(1, 2, 3));
                assert(ver1 == VersionNumber(1, 2, 4));
            \endcode
         */
        VersionNumber operator++(int)
        {
            const VersionNumber old = *this;
            operator++();
            return old;
        }

    private:
        friend class VersionConstraint;

        static constexpr size_t kMaxFields = 4;

        unsigned m_fieldsCount;
        unsigned m_fields[kMaxFields];
    };

    LIBRALF_EXPORT std::ostream &operator<<(std::ostream &s, const VersionNumber &ver);

    LIBRALF_EXPORT bool operator==(const VersionNumber &lhs, const VersionNumber &rhs);
    LIBRALF_EXPORT bool operator!=(const VersionNumber &lhs, const VersionNumber &rhs);
    LIBRALF_EXPORT bool operator<(const VersionNumber &lhs, const VersionNumber &rhs);
    LIBRALF_EXPORT bool operator>(const VersionNumber &lhs, const VersionNumber &rhs);
    LIBRALF_EXPORT bool operator<=(const VersionNumber &lhs, const VersionNumber &rhs);
    LIBRALF_EXPORT bool operator>=(const VersionNumber &lhs, const VersionNumber &rhs);

} // namespace LIBRALF_NS
