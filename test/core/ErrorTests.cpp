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

#include "Error.h"

#include <gtest/gtest.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

TEST(ErrorTest, StdErrorCode)
{
    const std::error_code err1 = ErrorCode::None;
    EXPECT_FALSE(err1);
    EXPECT_STREQ("libralf", err1.category().name());
    EXPECT_EQ("No error", err1.message());

    const std::error_code err2 = ErrorCode::InvalidPackage;
    EXPECT_TRUE(err2);
    EXPECT_STREQ("libralf", err2.category().name());
    EXPECT_EQ("Invalid package", err2.message());
    EXPECT_EQ(err2.value(), static_cast<int>(ErrorCode::InvalidPackage));
}

TEST(ErrorTest, EmptyError)
{
    Error error;
    EXPECT_FALSE(error);
    EXPECT_STREQ("", error.what());
}

TEST(ErrorTest, CustomError)
{
    Error error(ErrorCode::InvalidPackage, "Custom error message");
    EXPECT_TRUE(error);
    EXPECT_EQ(error.code(), ErrorCode::InvalidPackage);
    EXPECT_STREQ("Custom error message", error.what());

    error.assign(std::make_error_code(std::errc::bad_file_descriptor), "Custom error message 2");
    EXPECT_TRUE(error);
    EXPECT_EQ(error.code(), std::errc::bad_file_descriptor);
    EXPECT_STREQ("Custom error message 2", error.what());

    error.clear();
    EXPECT_FALSE(error);
    EXPECT_STREQ("", error.what());
}

TEST(ErrorTest, FormatError)
{
    auto err1 = Error::format(ErrorCode::InvalidPackage, "%s %d %hu", "Barnie", 42, 1288);
    EXPECT_TRUE(err1);
    EXPECT_EQ(err1.code(), ErrorCode::InvalidPackage);
    EXPECT_STREQ(err1.what(), "Barnie 42 1288");

    auto formatWithArgs = [](const char *fmt, ...) -> Error
    {
        std::va_list args;
        va_start(args, fmt);
        Error err = Error::format(std::make_error_code(std::errc::connection_reset), fmt, args);
        va_end(args);

        return err;
    };

    auto err2 = formatWithArgs("%s %d %hu", "Fruit loops", 42, 1288);
    EXPECT_TRUE(err2);
    EXPECT_EQ(err2.code(), std::errc::connection_reset);
    EXPECT_STREQ(err2.what(), "Fruit loops 42 1288");
}
