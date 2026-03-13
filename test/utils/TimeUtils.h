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

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#define DEFAULT_TIME_FORMAT "%Y-%m-%dT%H:%M:%S"

std::chrono::system_clock::time_point toSystemTimePoint(const std::string &str,
                                                        const std::string &format = DEFAULT_TIME_FORMAT);
std::string fromSystemTimePoint(const std::chrono::system_clock::time_point &tp,
                                const std::string &format = DEFAULT_TIME_FORMAT);

namespace testing::internal
{
    template <>
    class UniversalPrinter<std::chrono::system_clock::time_point>
    {
    public:
        static void Print(const std::chrono::system_clock::time_point &tp, ::std::ostream *os)
        {
            *os << fromSystemTimePoint(tp);
        }
    };
} // namespace testing::internal