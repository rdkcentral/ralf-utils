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

#include "VersionNumber.h"

#include <iostream>
#include <limits>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

VersionNumber::VersionNumber()
    : m_fieldsCount(1)
    , m_fields{ 0, 0, 0, 0 }
{
}

VersionNumber::VersionNumber(unsigned v0)
    : m_fieldsCount(1)
    , m_fields{ v0, 0, 0, 0 }
{
}

VersionNumber::VersionNumber(unsigned v0, unsigned v1)
    : m_fieldsCount(2)
    , m_fields{ v0, v1, 0, 0 }
{
}

VersionNumber::VersionNumber(unsigned v0, unsigned v1, unsigned v2)
    : m_fieldsCount(3)
    , m_fields{ v0, v1, v2, 0 }
{
}

VersionNumber::VersionNumber(unsigned v0, unsigned v1, unsigned v2, unsigned v3)
    : m_fieldsCount(4)
    , m_fields{ v0, v1, v2, v3 }
{
}

size_t VersionNumber::fieldCount() const
{
    return m_fieldsCount;
}

unsigned VersionNumber::fieldValue(size_t indexField) const
{
    if (indexField < kMaxFields)
        return m_fields[indexField];
    else
        return 0;
}

VersionNumber &VersionNumber::operator++()
{
    if (m_fieldsCount == 0)
        return *this;

    for (size_t i = m_fieldsCount; i > 0; --i)
    {
        if (m_fields[i - 1] < std::numeric_limits<unsigned>::max())
        {
            m_fields[i - 1]++;
            break;
        }
        else
        {
            m_fields[i - 1] = 0;
        }
    }

    return *this;
}

Result<VersionNumber> VersionNumber::fromString(std::string_view str)
{
    unsigned fieldIndex = 0;
    unsigned fields[kMaxFields] = { 0, 0, 0, 0 };

    char segment[32] = { 0 };
    size_t segmentIndex = 0;

    for (char ch : str)
    {
        if (ch == '.')
        {
            if (fieldIndex >= kMaxFields)
                return Error(ErrorCode::VersionNumberTooManyFields, "Too many fields in version number");

            if (segmentIndex == 0)
                return Error(ErrorCode::VersionNumberInvalidCharacter, "Empty field in version number");

            // We don't allow leading '0's in the segment
            if ((segmentIndex > 1) && (segment[0] == '0'))
                return Error(ErrorCode::VersionNumberInvalidCharacter, "Leading zero in version number field");

            segment[segmentIndex] = '\0';
            segmentIndex = 0;

            unsigned long value = strtoul(segment, nullptr, 10);
            if (value >= UINT32_MAX)
                return Error(ErrorCode::VersionNumberInvalidCharacter, "Field value too large");

            fields[fieldIndex++] = static_cast<unsigned>(value);
        }
        else if (isdigit(ch))
        {
            if (segmentIndex >= (sizeof(segment) - 2))
                return Error(ErrorCode::VersionNumberTooLong, "Version number field too long");

            segment[segmentIndex++] = ch;
        }
        else
        {
            return Error(ErrorCode::VersionNumberInvalidCharacter, "Invalid character in version number");
        }
    }

    if (segmentIndex > 0)
    {
        if (fieldIndex >= kMaxFields)
            return Error(ErrorCode::VersionNumberTooManyFields, "Too many fields in version number");

        // We don't allow leading '0's in the segment
        if ((segmentIndex > 1) && (segment[0] == '0'))
            return Error(ErrorCode::VersionNumberInvalidCharacter, "Leading zero in version number field");

        segment[segmentIndex] = '\0';
        fields[fieldIndex++] = std::stoul(segment);
    }

    switch (fieldIndex)
    {
        case 1:
            return VersionNumber(fields[0]);
        case 2:
            return VersionNumber(fields[0], fields[1]);
        case 3:
            return VersionNumber(fields[0], fields[1], fields[2]);
        case 4:
            return VersionNumber(fields[0], fields[1], fields[2], fields[3]);
        default:
            return Error(ErrorCode::VersionNumberEmpty, "Empty version number");
    }
}

std::string VersionNumber::toString() const
{
    char buffer[128];

    if (m_fieldsCount == 1)
        snprintf(buffer, sizeof(buffer), "%u", m_fields[0]);
    else if (m_fieldsCount == 2)
        snprintf(buffer, sizeof(buffer), "%u.%u", m_fields[0], m_fields[1]);
    else if (m_fieldsCount == 3)
        snprintf(buffer, sizeof(buffer), "%u.%u.%u", m_fields[0], m_fields[1], m_fields[2]);
    else if (m_fieldsCount == 4)
        snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", m_fields[0], m_fields[1], m_fields[2], m_fields[3]);
    else
        buffer[0] = '\0';

    return buffer;
}

std::ostream &LIBRALF_NS::operator<<(std::ostream &s, const VersionNumber &ver)
{
    s << ver.toString();
    return s;
}

int VersionNumber::compare(const VersionNumber &lhs, const VersionNumber &rhs)
{
    const unsigned n = std::min(lhs.m_fieldsCount, rhs.m_fieldsCount);
    for (unsigned fc = 0; fc < n; fc++)
    {
        if (lhs.m_fields[fc] < rhs.m_fields[fc])
            return -1;
        else if (lhs.m_fields[fc] > rhs.m_fields[fc])
            return 1;
    }

    if (lhs.m_fieldsCount > n)
    {
        // lhs has more fields than rhs - if all the extra fields are 0, then they are equal
        for (unsigned fc = n; fc < lhs.m_fieldsCount; fc++)
        {
            if (lhs.m_fields[fc] != 0)
                return 1;
        }
    }
    else if (rhs.m_fieldsCount > n)
    {
        // rhs has more fields than lhs - if all the extra fields are 0, then they are equal
        for (unsigned fc = n; fc < rhs.m_fieldsCount; fc++)
        {
            if (rhs.m_fields[fc] != 0)
                return -1;
        }
    }

    // they are equal
    return 0;
}

bool LIBRALF_NS::operator==(const VersionNumber &lhs, const VersionNumber &rhs)
{
    return VersionNumber::compare(lhs, rhs) == 0;
}

bool LIBRALF_NS::operator!=(const VersionNumber &lhs, const VersionNumber &rhs)
{
    return VersionNumber::compare(lhs, rhs) != 0;
}

bool LIBRALF_NS::operator<(const VersionNumber &lhs, const VersionNumber &rhs)
{
    return VersionNumber::compare(lhs, rhs) < 0;
}

bool LIBRALF_NS::operator>(const VersionNumber &lhs, const VersionNumber &rhs)
{
    return VersionNumber::compare(lhs, rhs) > 0;
}

bool LIBRALF_NS::operator<=(const VersionNumber &lhs, const VersionNumber &rhs)
{
    return VersionNumber::compare(lhs, rhs) <= 0;
}

bool LIBRALF_NS::operator>=(const VersionNumber &lhs, const VersionNumber &rhs)
{
    return VersionNumber::compare(lhs, rhs) >= 0;
}
