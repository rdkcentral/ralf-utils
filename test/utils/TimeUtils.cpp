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

#include "TimeUtils.h"
#include "date/date.h"

#include <sstream>

using namespace testing::internal;

// -----------------------------------------------------------------------------
/*!
    Helper function to convert a time string in XX format to a system time point.

    If the function fails then a default constructed time point is returned.

 */
std::chrono::system_clock::time_point toSystemTimePoint(const std::string &str, const std::string &format)
{
    std::istringstream in(str);

    std::chrono::system_clock::time_point tp;
    in >> date::parse(format, tp);

    return tp;
}

// -----------------------------------------------------------------------------
/*!
    Helper function to convert a time point to a string.

    This is used as a pretty printer for the gtest framework.

 */
std::string fromSystemTimePoint(const std::chrono::system_clock::time_point &tp, const std::string &format)
{
    return date::format(format, tp);
}
