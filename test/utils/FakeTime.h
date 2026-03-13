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

#include <chrono>
#include <ctime>
#include <string>

// -------------------------------------------------------------------------
/*!
    \class FakeTime
    \brief Test helper class to replace _some_ of the posix time functions
    to fake the time.

    It's purpose is to allow tests to fake the current time, so that they
    test things like ignoring certificate expiry.

    This is a very basic time replacement class, it only replaces the high
    level posix time functions like time() and gettimeofday().  It is enough
    for openssl to get the current time, but if the crypto library is changed
    or openssl changes then this class may need to be updated.


 */
class FakeTime
{
public:
    explicit FakeTime(time_t time);
    explicit FakeTime(std::chrono::system_clock::time_point time);
    explicit FakeTime(int year, int month = 0, int day = 0, int hour = 0, int minute = 0, int second = 0);
    explicit FakeTime(const std::string &str, const std::string &format = "%Y-%m-%dT%H:%M:%S");

    ~FakeTime();
};
