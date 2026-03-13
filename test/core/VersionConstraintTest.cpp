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

#include <gtest/gtest.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

TEST(VersionConstraintTest, NoConstraint)
{
    {
        auto constraint = VersionConstraint::fromString("");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1)));
    }

    {
        auto constraint = VersionConstraint::fromString("*");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1)));
    }
}

TEST(VersionConstraintTest, PrefixConstraint)
{
    {
        auto constraint = VersionConstraint::fromString("1");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 4)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1)));
    }

    {
        auto constraint = VersionConstraint::fromString("1.2");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1)));
    }
}

TEST(VersionConstraintTest, TildeConstraint)
{
    {
        auto constraint = VersionConstraint::fromString("~1.2.3");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(2)));
    }

    {
        auto constraint = VersionConstraint::fromString("~1.2");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 0)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(2)));
    }
}

TEST(VersionConstraintTest, CaretConstraint)
{
    {
        auto constraint = VersionConstraint::fromString("^1.2.3");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(2)));
    }

    {
        auto constraint = VersionConstraint::fromString("^1.2");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 0)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 9, 9, 9)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(2)));
    }

    {
        auto constraint = VersionConstraint::fromString("^1");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 0, 0)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(2)));
    }
}

TEST(VersionConstraintTest, EqualConstraint)
{
    {
        auto constraint = VersionConstraint::fromString("=1.2.3");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
    }

    {
        auto constraint = VersionConstraint::fromString("=1");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
    }

    {
        auto constraint = VersionConstraint::fromString("=1.2");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
    }
}

TEST(VersionConstraintTest, GreaterThanConstraint)
{
    {
        auto constraint = VersionConstraint::fromString(">1.2.3");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1)));
    }

    {
        auto constraint = VersionConstraint::fromString(">1");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 0, 0)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 0, 0, 1)));
    }

    {
        auto constraint = VersionConstraint::fromString(">1.2");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
    }
}

TEST(VersionConstraintTest, LessThanConstraint)
{
    {
        auto constraint = VersionConstraint::fromString("<1.2.3");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1)));
    }

    {
        auto constraint = VersionConstraint::fromString("<1");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(0)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 0, 0)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
    }

    {
        auto constraint = VersionConstraint::fromString("<1.2");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
    }
}

TEST(VersionConstraintTest, GreaterThanOrEqualConstraint)
{
    {
        auto constraint = VersionConstraint::fromString(">=1.2.3");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1)));
    }

    {
        auto constraint = VersionConstraint::fromString(">=1");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(0)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 0, 0, 1)));
    }

    {
        auto constraint = VersionConstraint::fromString(">=1.2");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 1)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
    }
}

TEST(VersionConstraintTest, LessThanOrEqualConstraint)
{
    {
        auto constraint = VersionConstraint::fromString("<=1.2.3");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1)));
    }

    {
        auto constraint = VersionConstraint::fromString("<=1");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(0)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3, 0)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 0, 0, 1)));
    }

    {
        auto constraint = VersionConstraint::fromString("<=1.2");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 1)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 1, 9, 9)));
    }
}

TEST(VersionConstraintTest, RangeConstraints)
{
    {
        auto constraint = VersionConstraint::fromString("1.2.3 - 1.3.0");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 3)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 3, 1)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 2)));
    }

    {
        auto constraint = VersionConstraint::fromString("1.2.3 - 1.3");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 3)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 3, 9)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 2, 2)));
    }

    {
        auto constraint = VersionConstraint::fromString("1.2 - 1.3.5.6");
        EXPECT_TRUE(constraint.hasValue()) << constraint.error().what();

        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2)));
        EXPECT_TRUE(constraint->isSatisfiedBy(VersionNumber(1, 2, 4)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 3, 5, 7)));
        EXPECT_FALSE(constraint->isSatisfiedBy(VersionNumber(1, 4)));
    }

    {
        auto constraint = VersionConstraint::fromString("1.3.9.9 - 1.3.9.8");
        EXPECT_TRUE(constraint.isError());
    }
}

TEST(VersionConstraintTest, InvalidConstraints)
{
    {
        auto constraint = VersionConstraint::fromString("abc");
        EXPECT_TRUE(constraint.isError());
    }

    {
        auto constraint = VersionConstraint::fromString(">==1.2.3");
        EXPECT_TRUE(constraint.isError());
    }

    {
        auto constraint = VersionConstraint::fromString(">");
        EXPECT_TRUE(constraint.isError());
    }

    {
        auto constraint = VersionConstraint::fromString(">1.a.3");
        EXPECT_TRUE(constraint.isError());
    }

    {
        auto constraint = VersionConstraint::fromString("1.2.3 - 1.3.0 - 1.4");
        EXPECT_TRUE(constraint.isError());
    }

    {
        auto constraint = VersionConstraint::fromString("1.2.3 > 1.3.0");
        EXPECT_TRUE(constraint.isError());
    }
}
