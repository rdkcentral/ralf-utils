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
#include "VersionNumber.h"

#include <string>
#include <string_view>
#include <vector>

namespace LIBRALF_NS
{

    // -----------------------------------------------------------------------------
    /*!
        \class VersionConstraint
        \brief Object stores a strict semantic version constraint.

        This is typically returned in a dependency list to describe the range of
        versions that are acceptable for a dependency.

     */
    class LIBRALF_EXPORT VersionConstraint
    {
    public:
        VersionConstraint();
        ~VersionConstraint();

        VersionConstraint(const VersionConstraint &other) = default;
        VersionConstraint(VersionConstraint &&other) noexcept = default;

        VersionConstraint &operator=(const VersionConstraint &other) = default;
        VersionConstraint &operator=(VersionConstraint &&other) noexcept = default;

        // -------------------------------------------------------------------------
        /*!
            \static

            Converts a string representation a version constraint to one of these
            objects.  Supports the following formats:
                - ""        No constraint, match any version
                - "*"       No constraint, match any version
                - "1.2.3"   Must match version exactly
                - "=1.2.3"  Must match version exactly
                - ">1.2.3"  Must be greater than version
                - ">=1.2.3" Must be greater than or equal version
                - "<1.2.3"  Must be less than version
                - "<=1.2.3" Must be less than or equal to version
                - "~1.2.3"  Must be greater than or equal to version, but less than the next
                            patch version.  So "~1.2.3" is equivalent to ">=1.2.3 <1.3.0"
                - "^1.2.3"  Must be greater than or equal to version, but less than the next
                            major version.  So "^1.2.3" is equivalent to ">=1.2.3 <2.0.0"
         */
        static Result<VersionConstraint> fromString(std::string_view str);

        // -------------------------------------------------------------------------
        /*!
            Converts the version number to a string representation.

            This may not be the exact string used to create the object, but it will
            be a valid string representation of the version constraint.  For example
            "~1.2.4" may be returned as "1.2.4 - 1.3.0"

         */
        std::string toString() const;

        // -------------------------------------------------------------------------
        /*!
            Checks if \a version satisfies the constraint.

         */
        bool isSatisfiedBy(const VersionNumber &version) const;

    private:
        friend bool operator==(const VersionConstraint &lhs, const VersionConstraint &rhs);
        friend bool operator!=(const VersionConstraint &lhs, const VersionConstraint &rhs);

        struct Constraint
        {
            enum class Type
            {
                GreaterThan,
                GreaterThanOrEqual,
                LessThan,
                LessThanOrEqual,
                Equal
            } type;
            VersionNumber version;

            bool operator==(const Constraint &rhs) const noexcept
            {
                return (type == rhs.type) && (version == rhs.version);
            }
        };

        std::vector<Constraint> m_constraints;
    };

    LIBRALF_EXPORT std::ostream &operator<<(std::ostream &s, const VersionConstraint &constraint);

    LIBRALF_EXPORT bool operator==(const VersionConstraint &lhs, const VersionConstraint &rhs);
    LIBRALF_EXPORT bool operator!=(const VersionConstraint &lhs, const VersionConstraint &rhs);

} // namespace LIBRALF_NS
