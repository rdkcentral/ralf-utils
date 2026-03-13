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

#include "dm-verity/UDevNotifySemaphore.h"

#include <gtest/gtest.h>

using namespace ::testing;
using namespace std::chrono_literals;
using namespace entos::ralf::dmverity;

#define DM_COOKIE_MAGIC 0x0D4D
#define DM_UDEV_FLAGS_SHIFT 16

// NOLINTNEXTLINE
class UDevNotifySemaphoreTests : public ::testing::Test
{
protected:
    void TearDown() override {}
};

TEST_F(UDevNotifySemaphoreTests, createCreatesSemaphoreSuccessfully)
{
    auto result = UDevNotifySemaphore::create();
    ASSERT_TRUE(result) << "failed to create UDevNotifySemaphore - " << result.error().what();

    auto semaphore = std::move(result.value());
    ASSERT_NE(semaphore.id(), -1) << "invalid semaphore ID returned";

    EXPECT_EQ(semaphore.cookie() >> 16, DM_COOKIE_MAGIC) << "invalid cookie magic";
}

TEST_F(UDevNotifySemaphoreTests, waitForUDevNotificationSucceeds)
{
    auto result = UDevNotifySemaphore::create();
    ASSERT_TRUE(result) << "failed to create UDevNotifySemaphore - " << result.error().what();

    auto semaphore = std::move(result.value());
    ASSERT_NE(semaphore.id(), -1) << "invalid semaphore ID returned";

    EXPECT_TRUE(semaphore.wait(10ms).isError());

    const uint32_t cookie = semaphore.cookie();
    EXPECT_EQ((cookie >> 16), DM_COOKIE_MAGIC) << "invalid cookie magic";

    int semId = semget(static_cast<key_t>(cookie), 1, 0);
    ASSERT_GE(semId, 0) << "failed to get semaphore ID from cookie: " << strerror(errno);

    int val = semctl(semaphore.id(), 0, GETVAL);
    EXPECT_EQ(val, 1) << "semaphore value should be 1 before notification:" << strerror(errno);

    struct sembuf sb = { 0, -1, IPC_NOWAIT };
    EXPECT_EQ(semop(semaphore.id(), &sb, 1), 0) << "failed to decrement semaphore: " << strerror(errno);

    EXPECT_TRUE(semaphore.wait(10ms));
}
