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

#include <gtest/gtest.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

TEST(VersionNumberTest, ToString)
{
    {
        VersionNumber version(1);
        EXPECT_EQ(version.toString(), "1");
    }

    {
        VersionNumber version(1, 2, 3);
        EXPECT_EQ(version.toString(), "1.2.3");
    }

    {
        VersionNumber version(0, 0, 0);
        EXPECT_EQ(version.toString(), "0.0.0");
    }

    {
        VersionNumber version(1, 2, 3, 4);
        EXPECT_EQ(version.toString(), "1.2.3.4");
    }

    {
        VersionNumber version(1, INT32_MAX, 3);
        EXPECT_EQ(version.toString(), "1.2147483647.3");
    }
}

TEST(VersionNumberTest, Compare)
{
    {
        VersionNumber ver1(1, 2, 3);
        VersionNumber ver2(1, 0, 4);
        EXPECT_EQ(VersionNumber::compare(ver1, ver2), 1);
        EXPECT_NE(ver1, ver2);
        EXPECT_GT(ver1, ver2);
    }

    {
        VersionNumber ver1(1, 2, 3);
        VersionNumber ver2(1, 2, 4);
        EXPECT_EQ(VersionNumber::compare(ver1, ver2), -1);
        EXPECT_NE(ver1, ver2);
        EXPECT_LT(ver1, ver2);
    }

    {
        VersionNumber ver1(0, 2, 3);
        VersionNumber ver2(0, 2, 3);
        EXPECT_EQ(VersionNumber::compare(ver1, ver2), 0);
        EXPECT_EQ(ver1, ver2);
    }

    {
        VersionNumber ver1(1);
        VersionNumber ver2(1, 0, 0, 0);
        EXPECT_EQ(VersionNumber::compare(ver1, ver2), 0);
        EXPECT_EQ(ver1, ver2);
    }
}

TEST(VersionNumberTest, Increment)
{
    {
        VersionNumber version(1, 2, 3);
        EXPECT_EQ(++version, VersionNumber(1, 2, 4));
        EXPECT_EQ(version++, VersionNumber(1, 2, 4));
    }

    {
        VersionNumber version(1, 2, std::numeric_limits<unsigned>::max());
        EXPECT_EQ(++version, VersionNumber(1, 3));
    }

    {
        VersionNumber version(1, std::numeric_limits<unsigned>::max());
        EXPECT_EQ(++version, VersionNumber(2));
    }

    {
        VersionNumber version(std::numeric_limits<unsigned>::max());
        EXPECT_EQ(++version, VersionNumber(0));
    }
}
