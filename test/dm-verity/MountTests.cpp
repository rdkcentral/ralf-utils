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

#include "FileUtils.h"
#include "MockFile.h"
#include "MockMount.h"
#include "dm-verity/DmVerityMount.h"

#include <gtest/gtest.h>

#include <map>

#include <fcntl.h>
#if defined(__linux__)
#    include <mntent.h>
#elif defined(__APPLE__)
#    include <sys/mount.h>
#endif

using namespace ::testing;
using namespace entos::ralf::dmverity;

// NOLINTNEXTLINE
class MountTests : public ::testing::Test
{
protected:
    void TearDown() override { MockFile::clearAllMocks(); }

protected:
#if defined(__linux__)
    static std::map<std::filesystem::path, std::filesystem::path> getMountedBlockDevices()
    {
        auto mountsFile = fopen("/proc/mounts", "re");
        EXPECT_NE(mountsFile, nullptr);

        struct mntent *mnt;
        std::map<std::filesystem::path, std::filesystem::path> mounts;
        while ((mnt = getmntent(mountsFile)) != nullptr)
        {
            if ((mnt->mnt_fsname[0] == '/') && std::filesystem::is_block_file(mnt->mnt_fsname))
                mounts[mnt->mnt_fsname] = mnt->mnt_dir;
        }

        fclose(mountsFile);
        return mounts;
    }
#elif defined(__APPLE__)
    static std::map<std::filesystem::path, std::filesystem::path> getMountedBlockDevices()
    {
        struct statfs *mounts;
        int numMounts = getmntinfo(&mounts, MNT_WAIT);
        EXPECT_GE(numMounts, 0);

        std::map<std::filesystem::path, std::filesystem::path> deviceMounts;
        for (int i = 0; i < numMounts; i++)
        {
            deviceMounts[mounts[i].f_fstypename] = mounts[i].f_mntonname;
        }

        return deviceMounts;
    }
#endif

    static std::set<std::filesystem::path> getAllBlockDevices()
    {
        std::set<std::filesystem::path> blockDevices;

        const auto devDir = std::filesystem::path("/dev");
        for (const auto &entry : std::filesystem::directory_iterator(devDir))
        {
            if (std::filesystem::is_block_file(entry.path()))
                blockDevices.insert(entry.path());
        }

        return blockDevices;
    }
};

TEST_F(MountTests, testBasics)
{
    // Skip the test if we couldn't find any mounted block devices
    const auto mountedBlockDevices = getMountedBlockDevices();
    if (mountedBlockDevices.empty())
    {
        GTEST_SKIP() << "No mounted block devices found, skipping detach() test";
    }

    auto mountedBlockDevice = *mountedBlockDevices.begin();

    DmVerityMount mount(mountedBlockDevice.first, mountedBlockDevice.second, "testVolume", "testUuid");
    EXPECT_EQ(mount.volumeName(), "testVolume");
    EXPECT_EQ(mount.volumeUuid(), "testUuid");
    EXPECT_EQ(mount.mountPoint(), mountedBlockDevice.second);

    EXPECT_CALL(*MockMount::get(), umount2(StrEq(mountedBlockDevice.second.c_str()), UMOUNT_NOFOLLOW));
}

TEST_F(MountTests, testIsMountedCheck)
{
    // This test checks the isMounted() method of DmVerityMount class returns a correct value.  We can't actually
    // perform a real mount in the test, so we cheat a bit by finding a mounted and unmounted block device and using
    // those to test the method.

    // Skip the test if we couldn't find any mounted block devices
    const auto mountedBlockDevices = getMountedBlockDevices();
    if (mountedBlockDevices.empty())
    {
        GTEST_SKIP() << "No mounted block devices found, skipping isMounted() test";
    }

    const auto blockDevices = getAllBlockDevices();
    if (blockDevices.size() < 2)
    {
        GTEST_SKIP() << "We need at least 2 block devices for this test, skipping isMounted() test";
    }

    {
        auto mountedBlockDevice = *mountedBlockDevices.begin();

        // Create a DmVerityMount instance for the found device
        DmVerityMount mount(mountedBlockDevice.first, mountedBlockDevice.second, "testVolume", "testUuid");
        EXPECT_TRUE(mount.isMounted());

        // Expect call to umount2 when destructing the object
        EXPECT_CALL(*MockMount::get(), umount2(StrEq(mountedBlockDevice.second.c_str()), UMOUNT_NOFOLLOW))
            .WillOnce(Return(0));
    }

    {
        auto mountedBlockDevice = *mountedBlockDevices.begin();

        // Find a different block device
        auto differentBlockDevice = std::find_if(blockDevices.begin(), blockDevices.end(),
                                                 [&mountedBlockDevice](const std::filesystem::path &device)
                                                 { return device != mountedBlockDevice.first; });
        ASSERT_NE(differentBlockDevice, blockDevices.end());

        // Create a DmVerityMount with mismatched block devices
        DmVerityMount mount(*differentBlockDevice, mountedBlockDevice.second, "testVolume", "testUuid");
        EXPECT_FALSE(mount.isMounted());

        EXPECT_CALL(*MockMount::get(), umount2(StrEq(mountedBlockDevice.second.c_str()), UMOUNT_NOFOLLOW))
            .WillOnce(Return(0));
    }
}

TEST_F(MountTests, testDetach)
{
    // This test checks that calling detach() on the DmVerityMount prevents the unmount from happening when the object
    // is destroyed.

    // Skip the test if we couldn't find any mounted block devices
    const auto mountedBlockDevices = getMountedBlockDevices();
    if (mountedBlockDevices.empty())
    {
        GTEST_SKIP() << "No mounted block devices found, skipping detach() test";
    }

    auto mountedBlockDevice = *mountedBlockDevices.begin();

    {
        DmVerityMount mount(mountedBlockDevice.first, mountedBlockDevice.second, "testVolume", "testUuid");

        EXPECT_CALL(*MockMount::get(), umount2(StrEq(mountedBlockDevice.second.c_str()), UMOUNT_NOFOLLOW)).Times(1);
    }

    {
        DmVerityMount mount(mountedBlockDevice.first, mountedBlockDevice.second, "testVolume", "testUuid");
        mount.detach();

        EXPECT_CALL(*MockMount::get(), umount2(StrEq(mountedBlockDevice.second.c_str()), UMOUNT_NOFOLLOW)).Times(0);
    }
}

TEST_F(MountTests, testUnmount)
{
    // Skip the test if we couldn't find any mounted block devices
    const auto mountedBlockDevices = getMountedBlockDevices();
    if (mountedBlockDevices.empty())
    {
        GTEST_SKIP() << "No mounted block devices found, skipping detach() test";
    }

    auto mountedBlockDevice = *mountedBlockDevices.begin();

    {
        DmVerityMount mount(mountedBlockDevice.first, mountedBlockDevice.second, "testVolume", "testUuid");

        EXPECT_CALL(*MockMount::get(), umount2(StrEq(mountedBlockDevice.second.c_str()), UMOUNT_NOFOLLOW)).Times(1);

        mount.unmount();
    }
}
