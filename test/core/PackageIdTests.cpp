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

#include "core/Utils.h"

#include <gtest/gtest.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

TEST(PackageIdTest, validPackageId)
{
    std::vector<std::string> validIds = {
        "com.bskyb.epgui", "com.bskyb.news", "blah", "pac-man", "sky_news", "com.1234-some_thing",
    };

    for (const auto &id : validIds)
    {
        EXPECT_TRUE(verifyPackageId(id));
    }
}

TEST(PackageIdTest, invalidPackageId)
{
    std::vector<std::string> invalidIds = { "this appid is invalid",
                                            "_cant_start_with_underscore",
                                            "-cant_start_with_dash",
                                            ".cant.start.with.a.dot",
                                            "cant.include.double..dot",
                                            "cant_end_with_underscore_",
                                            "cant_end_with_underscore-",
                                            "cant.end.with.a.dot.",
                                            "../haha/you/wish/",
                                            "../../../../",
                                            "\"../../\"",
                                            "^^What.is.that?",
                                            "" };

    for (const auto &id : invalidIds)
    {
        EXPECT_FALSE(verifyPackageId(id));
    }
}
