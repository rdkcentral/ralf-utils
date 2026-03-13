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

#include <gtest/gtest.h>

#include <filesystem>

/// This is not a real test, it is just a check to see if the test data is present and the correct size.
/// A common test failure for first time users is that they haven't checked out the code with git LFS enabled
/// meaning the test data is missing or the wrong size.
TEST(TestData, checkTestDataSize)
{
    const std::filesystem::path testDataDir = TEST_DATA_DIR;
    ASSERT_TRUE(std::filesystem::exists(testDataDir / "simple.wgt"))
        << "Test data not found - have you run `git lfs install` and `git lfs pull`?";
    ASSERT_EQ(std::filesystem::file_size(testDataDir / "simple.wgt"), 6999)
        << "Invalid size of testdata - have you run `git lfs install` and `git lfs pull`?";
}
