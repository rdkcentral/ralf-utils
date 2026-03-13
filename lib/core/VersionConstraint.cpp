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

#include "VersionConstraint.h"

#include <iostream>
#include <regex>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

VersionConstraint::VersionConstraint() // NOLINT(modernize-use-equals-default)
{
}

VersionConstraint::~VersionConstraint() // NOLINT(modernize-use-equals-default)
{
}

bool VersionConstraint::isSatisfiedBy(const VersionNumber &version) const
{
    for (const auto &constraint : m_constraints)
    {
        switch (constraint.type)
        {
            case Constraint::Type::GreaterThan:
                if (version <= constraint.version)
                    return false;
                break;
            case Constraint::Type::GreaterThanOrEqual:
                if (version < constraint.version)
                    return false;
                break;
            case Constraint::Type::LessThan:
                if (version >= constraint.version)
                    return false;
                break;
            case Constraint::Type::LessThanOrEqual:
                if (version > constraint.version)
                    return false;
                break;
            case Constraint::Type::Equal:
                if (version != constraint.version)
                    return false;
                break;
            default:
                break;
        }
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    This code loosely follows https://devhints.io/semver, but we only support
    a single constraint, not a list of constraints.

 */
Result<VersionConstraint> VersionConstraint::fromString(std::string_view str)
{
    // Trim whitespace
    std::string trimmedStr(str);
    trimmedStr.erase(0, trimmedStr.find_first_not_of(" \t"));
    trimmedStr.erase(trimmedStr.find_last_not_of(" \t") + 1);

    if (trimmedStr.empty() || (trimmedStr == "*"))
    {
        // No constraint, match any version
        return VersionConstraint();
    }

    // Regex patterns for parsing constraints, we only support a single constraint
    // not a list
    std::smatch match;
    static const std::regex pattern(R"(^(>=|<=|>|<|=|\^|~)?\s*([\d\.]+)$)");
    static const std::regex rangePattern(R"(^([\d\.]+)\s*\-\s*([\d\.]+)$)");

    VersionConstraint constraint;

    if (std::regex_match(trimmedStr, match, rangePattern) && (match.size() == 3))
    {
        auto minVersion_ = VersionNumber::fromString(match[1].str());
        auto maxVersion_ = VersionNumber::fromString(match[2].str());
        if (!minVersion_ || !maxVersion_)
            return Error(ErrorCode::VersionConstraintInvalid, "Invalid version constraint format");

        auto minVersion = minVersion_.value();
        auto maxVersion = maxVersion_.value();
        if (minVersion > maxVersion)
            return Error(ErrorCode::VersionConstraintInvalid, "Invalid version constraint format");

        constraint.m_constraints.push_back({ Constraint::Type::GreaterThanOrEqual, minVersion });

        if (maxVersion.fieldCount() < VersionNumber::kMaxFields)
            constraint.m_constraints.push_back({ Constraint::Type::LessThan, ++maxVersion });
        else
            constraint.m_constraints.push_back({ Constraint::Type::LessThanOrEqual, maxVersion });
    }
    else if (std::regex_match(trimmedStr, match, pattern) && (match.size() == 3))
    {
        const std::string operatorStr = match[1].str();
        const std::string versionStr = match[2].str();

        // Parse the version number
        auto versionResult = VersionNumber::fromString(versionStr);
        if (!versionResult)
            return versionResult.error();

        VersionNumber version = versionResult.value();

        // Determine the type of constraint
        if (operatorStr == ">")
        {
            constraint.m_constraints.push_back({ Constraint::Type::GreaterThan, version });
        }
        else if (operatorStr == ">=")
        {
            constraint.m_constraints.push_back({ Constraint::Type::GreaterThanOrEqual, version });
        }
        else if (operatorStr == "<")
        {
            constraint.m_constraints.push_back({ Constraint::Type::LessThan, version });
        }
        else if (operatorStr == "<=")
        {
            constraint.m_constraints.push_back({ Constraint::Type::LessThanOrEqual, version });
        }
        else if (operatorStr == "=")
        {
            constraint.m_constraints.push_back({ Constraint::Type::Equal, version });
        }
        else if (operatorStr == "~")
        {
            // "~1.2.3" is equivalent to ">=1.2.3 <1.3.0"
            constraint.m_constraints.push_back({ Constraint::Type::GreaterThanOrEqual, version });
            constraint.m_constraints.push_back(
                { Constraint::Type::LessThan, VersionNumber(version.fieldValue(0), version.fieldValue(1) + 1) });
        }
        else if (operatorStr == "^")
        {
            // "^1.2.3" is equivalent to ">=1.2.3 <2.0.0"
            constraint.m_constraints.push_back({ Constraint::Type::GreaterThanOrEqual, version });
            constraint.m_constraints.push_back({ Constraint::Type::LessThan, VersionNumber(version.fieldValue(0) + 1) });
        }
        else
        {
            // if no operator is specified, we assume it's an exact match on the version numbers supplied
            constraint.m_constraints.push_back({ Constraint::Type::GreaterThanOrEqual, version });
            constraint.m_constraints.push_back({ Constraint::Type::LessThan, ++version });
        }
    }
    else
    {
        // Invalid constraint format
        return Error(ErrorCode::VersionConstraintInvalid, "Invalid version constraint format");
    }

    return constraint;
}

std::string VersionConstraint::toString() const
{
    if (m_constraints.empty())
        return "*";

    if (m_constraints.size() == 1)
    {
        std::string op;
        switch (m_constraints[0].type)
        {
            case Constraint::Type::GreaterThan:
                op = ">";
                break;
            case Constraint::Type::GreaterThanOrEqual:
                op = ">=";
                break;
            case Constraint::Type::LessThan:
                op = "<";
                break;
            case Constraint::Type::LessThanOrEqual:
                op = "<=";
                break;
            case Constraint::Type::Equal:
                op = "=";
                break;
        }

        return op + m_constraints[0].version.toString();
    }
    else
    {
        // If we have multiple constraints it means it is a range, and it's guaranteed the first constraint is the lower
        // bound and the last one is the upper bound
        return m_constraints[0].version.toString() + " - " + m_constraints[1].version.toString();
    }
}

std::ostream &LIBRALF_NS::operator<<(std::ostream &s, const VersionConstraint &constraint)
{
    s << constraint.toString();
    return s;
}

bool LIBRALF_NS::operator==(const VersionConstraint &lhs, const VersionConstraint &rhs)
{
    return (lhs.m_constraints == rhs.m_constraints);
}

bool LIBRALF_NS::operator!=(const VersionConstraint &lhs, const VersionConstraint &rhs)
{
    return (lhs.m_constraints != rhs.m_constraints);
}
