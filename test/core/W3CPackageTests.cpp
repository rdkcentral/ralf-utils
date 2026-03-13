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

#include "w3c-widget/W3CPackage.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

TEST(W3CPackageTest, TestIsW3CPackage)
{
    // Test a valid zip file
    {
        int fd = open(TEST_DATA_DIR "/simple.wgt", O_CLOEXEC | O_RDONLY);
        ASSERT_NE(fd, -1);

        auto package = Package::openWithoutVerification(fd, Package::OpenFlags::None);
        ASSERT_FALSE(package.isError()) << package.error().what();

        EXPECT_EQ(package->format(), Package::Format::Widget);

        close(fd);
    }

    // Test an invalid zip file
    {
        int fd = open(TEST_DATA_DIR "/simple.erofs.wgt", O_CLOEXEC | O_RDONLY);
        ASSERT_NE(fd, -1);

        auto package = Package::openWithoutVerification(fd, Package::OpenFlags::None);
        EXPECT_TRUE(package.isError());

        close(fd);
    }
}

TEST(W3CPackageTest, TestOpenW3CPackageWithoutVerification)
{
    Error err;

    // Test a valid widget file
    {
        int fd = open(TEST_DATA_DIR "/simple.wgt", O_CLOEXEC | O_RDONLY);
        ASSERT_NE(fd, -1);

        auto package = W3CPackage::open(fd, std::nullopt, Package::OpenFlags::None, &err);
        ASSERT_FALSE(err) << err.what();
        ASSERT_NE(package, nullptr);
        ASSERT_TRUE(package->isValid());

        auto metaData = package->metaData(&err);
        ASSERT_FALSE(err) << err.what();
        ASSERT_NE(metaData, nullptr);

        EXPECT_EQ(metaData->id(), "whatsmyua");
        EXPECT_EQ(metaData->versionName(), "1.0");
        EXPECT_EQ(metaData->type(), PackageType::Application);

        close(fd);
    }

    // Test an invalid zip file
    {
        int fd = open(TEST_DATA_DIR "/simple.erofs.wgt", O_CLOEXEC | O_RDONLY);
        ASSERT_NE(fd, -1);

        auto package = W3CPackage::open(fd, VerificationBundle(), Package::OpenFlags::None, &err);
        ASSERT_TRUE(err);
        ASSERT_EQ(package, nullptr);

        close(fd);
    }
}
