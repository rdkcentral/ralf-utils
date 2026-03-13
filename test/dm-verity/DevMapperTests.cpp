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
#include "StringUtils.h"
#include "dm-verity/DevMapper.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <linux/dm-ioctl.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <sys/sysmacros.h>
#include <unistd.h>

// It's unclear what this flag is for, libdevmapper always sets it so we do too.
#ifndef DM_EXISTS_FLAG
#    define DM_EXISTS_FLAG 0x00000004
#endif

// These are the flags for the event_nr field and are used by udev to determine how to handle the device.
// See https://salsa.debian.org/lvm-team/lvm2/-/blob/main/device_mapper/all.h
#define DM_COOKIE_MAGIC 0x0D4D
#define DM_UDEV_FLAGS_MASK 0xFFFF0000
#define DM_UDEV_FLAGS_SHIFT 16

#define DM_UDEV_DISABLE_DM_RULES_FLAG 0x0001
#define DM_UDEV_DISABLE_SUBSYSTEM_RULES_FLAG 0x0002
#define DM_UDEV_DISABLE_DISK_RULES_FLAG 0x0004
#define DM_UDEV_DISABLE_OTHER_RULES_FLAG 0x0008
#define DM_UDEV_LOW_PRIORITY_FLAG 0x0010
#define DM_UDEV_DISABLE_LIBRARY_FALLBACK 0x0020
#define DM_UDEV_PRIMARY_SOURCE_FLAG 0x0040

// These are hard-coded in our code
#define DATA_BLOCK_SIZE 4096u
#define HASH_BLOCK_SIZE 4096u

using namespace ::testing;
using namespace entos::ralf::dmverity;

// NOLINTNEXTLINE
class DevMapperTests : public ::testing::Test
{
protected:
    void TearDown() override { MockFile::clearAllMocks(); }
};

TEST_F(DevMapperTests, testIsAvailable)
{
    char controlPath[64];
    snprintf(controlPath, sizeof(controlPath), "/dev/%s/%s", DM_DIR, DM_CONTROL_NODE);
    auto mockCtrlDevice = MockFile::registerPath(controlPath);

    // check failure to open device
    {
        EXPECT_CALL(*mockCtrlDevice, open(StrEq(controlPath), O_CLOEXEC | O_RDWR, _)).WillOnce(Return(nullptr));

        DevMapper devMapper;
        EXPECT_FALSE(devMapper.isAvailable());
    }

    // check on failure public APIs always fail
    {
        EXPECT_CALL(*mockCtrlDevice, open(StrEq(controlPath), O_CLOEXEC | O_RDWR, _)).WillOnce(Return(nullptr));

        DevMapper devMapper;
        EXPECT_FALSE(devMapper.isAvailable());

        EXPECT_EQ(devMapper.version(), entos::ralf::VersionNumber(0));

        {
            const auto result = devMapper.mappedDevices();
            EXPECT_TRUE(result.isError());
            EXPECT_STREQ(result.error().what(), "Device mapper is not available");
        }

        {
            const auto result = devMapper.mapWithVerity(1, "/dev/loop0", "uuid", 4096, 4096, {}, false);
            EXPECT_TRUE(result.isError());
            EXPECT_STREQ(result.error().what(), "Device mapper is not available");
        }

        {
            const auto result = devMapper.unmap("name", "uuid");
            EXPECT_TRUE(result.isError());
            EXPECT_STREQ(result.error().what(), "Device mapper is not available");
        }

        {
            const auto result = devMapper.mapStatus("name", "uuid");
            EXPECT_TRUE(result.isError());
            EXPECT_STREQ(result.error().what(), "Device mapper is not available");
        }
    }

    // check failure to get version
    {
        auto mockOpenCtrlDevice = std::make_shared<MockOpenFile>();
        EXPECT_CALL(*mockOpenCtrlDevice, ioctl(DM_VERSION, _)).WillOnce(Return(-1));
        EXPECT_CALL(*mockOpenCtrlDevice, close()).WillOnce(Return(0));

        EXPECT_CALL(*mockCtrlDevice, open(StrEq(controlPath), O_CLOEXEC | O_RDWR, _)).WillOnce(Return(mockOpenCtrlDevice));

        DevMapper devMapper;
        EXPECT_FALSE(devMapper.isAvailable());
    }

    // check unsupported version
    {
        auto mockOpenCtrlDevice = std::make_shared<MockOpenFile>();
        EXPECT_CALL(*mockOpenCtrlDevice, ioctl(DM_VERSION, _))
            .WillOnce(
                [](unsigned long request, void *arg) -> int
                {
                    auto *cmd = static_cast<dm_ioctl *>(arg);
                    cmd->version[0] = 4;
                    cmd->version[1] = 5;
                    cmd->version[2] = 0;
                    return 0;
                });
        EXPECT_CALL(*mockOpenCtrlDevice, close()).WillOnce(Return(0));

        EXPECT_CALL(*mockCtrlDevice, open(StrEq(controlPath), O_CLOEXEC | O_RDWR, _)).WillOnce(Return(mockOpenCtrlDevice));

        DevMapper devMapper;
        EXPECT_FALSE(devMapper.isAvailable());
    }

    // check success
    {
        auto mockOpenCtrlDevice = std::make_shared<MockOpenFile>();
        EXPECT_CALL(*mockOpenCtrlDevice, ioctl(DM_VERSION, _))
            .WillOnce(
                [](unsigned long request, void *arg) -> int
                {
                    auto *cmd = static_cast<dm_ioctl *>(arg);
                    cmd->version[0] = 5;
                    cmd->version[1] = 6;
                    cmd->version[2] = 7;
                    return 0;
                });
        EXPECT_CALL(*mockOpenCtrlDevice, close()).WillOnce(Return(0));

        EXPECT_CALL(*mockCtrlDevice, open(StrEq(controlPath), O_CLOEXEC | O_RDWR, _)).WillOnce(Return(mockOpenCtrlDevice));

        DevMapper devMapper;
        EXPECT_TRUE(devMapper.isAvailable());
        EXPECT_EQ(devMapper.version(), entos::ralf::VersionNumber(5, 6, 7));
    }
}

// -------------------------------------------------------------------------
/*!
    Takes a real file at \a filePath and sets up a mock loopback device
    that uses that file as the backing store.

    This implements the basic ioctl / fstat / read operations needed to
    make the loop device appear to be a real loopback block device.

 */
class MockLoopbackDevice
{
public:
    explicit MockLoopbackDevice(const std::filesystem::path &filePath)
        : m_loopDevMinor(m_loopDeviceCounter++)
        , m_mockOpenFile(std::make_shared<MockOpenFile>())
    {
        // Create the mock loopback device file
        m_mockLoopPath = "/dev/loop" + std::to_string(m_loopDevMinor);
        m_mockLoopFile = MockFile::registerPath(m_mockLoopPath);

        EXPECT_CALL(*m_mockLoopFile, open(StrEq(m_mockLoopPath), O_CLOEXEC | O_RDWR, _)).WillOnce(Return(m_mockOpenFile));

        m_mockFd = open(m_mockLoopPath.c_str(), O_CLOEXEC | O_RDWR);

        // Open the real file to back the loop device
        m_realFd = open(filePath.c_str(), O_CLOEXEC | O_RDWR);
        EXPECT_GE(m_realFd, 0);

        // Set up the mock'ed functions to pretend to be a loop device
        EXPECT_CALL(*m_mockOpenFile, fstat(_))
            .Times(AnyNumber())
            .WillRepeatedly([this](struct stat *buf) -> int { return onFStatCall(buf); });
        EXPECT_CALL(*m_mockOpenFile, ioctl(_, _))
            .Times(AnyNumber())
            .WillRepeatedly([this](unsigned long request, void *arg) -> int { return onIoctlCall(request, arg); });

        EXPECT_CALL(*m_mockOpenFile, read(_, _))
            .Times(AnyNumber())
            .WillRepeatedly([this](void *buf, size_t nbytes) -> ssize_t { return onRead(buf, nbytes); });
        EXPECT_CALL(*m_mockOpenFile, pread(_, _, _))
            .Times(AnyNumber())
            .WillRepeatedly([this](void *buf, size_t nbytes, off_t offset) -> ssize_t
                            { return onReadOffset(buf, nbytes, offset); });

        // Don't support any write operations to the loop device
        EXPECT_CALL(*m_mockOpenFile, write(_, _)).Times(0);
        EXPECT_CALL(*m_mockOpenFile, pwrite(_, _, _)).Times(0);
    }

    ~MockLoopbackDevice()
    {
        m_mockLoopFile.reset();
        m_mockOpenFile.reset();

        if (m_realFd >= 0)
        {
            EXPECT_EQ(close(m_realFd), 0);
        }
    }

    int fd() const { return m_mockFd; }

    int majorNum() const { return m_loopDevMajor; }
    int minorNum() const { return m_loopDevMinor; }

private:
    int onFStatCall(struct stat *buf) const
    {
        memset(buf, 0, sizeof(struct stat));
        buf->st_mode = S_IFBLK;
        buf->st_size = lseek(m_realFd, 0, SEEK_END);
        buf->st_rdev = makedev(m_loopDevMajor, m_loopDevMinor);
        return 0;
    }

    int onIoctlCall(unsigned long request, void *arg) const
    {
        (void)m_realFd;

        if (request == BLKSSZGET)
        {
            int *blockSize = static_cast<int *>(arg);
            *blockSize = 4096;
            return 0;
        }

        return -1;
    }

    ssize_t onRead(void *buf, size_t nbytes) const { return ::read(m_realFd, buf, nbytes); }

    ssize_t onReadOffset(void *buf, size_t nbytes, off_t offset) const
    {
        return ::pread(m_realFd, buf, nbytes, offset);
    }

private:
    static std::atomic<int> m_loopDeviceCounter;
    const int m_loopDevMajor = 7;
    const int m_loopDevMinor;

    std::filesystem::path m_mockLoopPath;
    std::shared_ptr<MockFile> m_mockLoopFile;
    std::shared_ptr<MockOpenFile> m_mockOpenFile;

    int m_mockFd = -1;
    int m_realFd = -1;
};

std::atomic<int> MockLoopbackDevice::m_loopDeviceCounter = 0;

// -------------------------------------------------------------------------
/*!
    Creates a fake block device node in /dev/block/ with the given \a majorNum
    and \a minorNum.

    The created block device has no backing store, it's just there to allow
    the devmapper code to find the device node when it looks for it and run
    fstat on it.

 */
class MockBlockDevice
{
public:
    MockBlockDevice(int majorNum, int minorNum)
        : m_devNo(makedev(majorNum, minorNum))
    {
        // Create the mock loopback device file
        std::filesystem::path path = "/dev/block/" + std::to_string(majorNum) + ":" + std::to_string(minorNum);
        m_mockBlockDevFile = MockFile::registerPath(path);
        m_mockBlockDevOpenFile = std::make_shared<MockOpenFile>();

        EXPECT_CALL(*m_mockBlockDevFile, stat(StrEq(path), _))
            .Times(AnyNumber())
            .WillRepeatedly(
                [majorNum, minorNum](const char *path, struct stat *buf) -> int
                {
                    memset(buf, 0x00, sizeof(struct stat));
                    buf->st_mode = S_IFBLK;
                    buf->st_rdev = makedev(majorNum, minorNum);
                    return 0;
                });

        EXPECT_CALL(*m_mockBlockDevFile, open(StrEq(path), O_CLOEXEC | O_RDONLY, _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(m_mockBlockDevOpenFile));

        EXPECT_GE(open(path.c_str(), O_CLOEXEC | O_RDONLY), 0);

        // Set up the mock'ed functions to pretend to be a loop device
        EXPECT_CALL(*m_mockBlockDevOpenFile, fstat(_))
            .Times(AnyNumber())
            .WillRepeatedly(
                [majorNum, minorNum](struct stat *buf) -> int
                {
                    memset(buf, 0x00, sizeof(struct stat));
                    buf->st_mode = S_IFBLK;
                    buf->st_rdev = makedev(majorNum, minorNum);
                    return 0;
                });

        // Don't support any of these operations on the block device
        EXPECT_CALL(*m_mockBlockDevOpenFile, ioctl(_, _)).Times(0);
        EXPECT_CALL(*m_mockBlockDevOpenFile, read(_, _)).Times(0);
        EXPECT_CALL(*m_mockBlockDevOpenFile, pread(_, _, _)).Times(0);
        EXPECT_CALL(*m_mockBlockDevOpenFile, write(_, _)).Times(0);
        EXPECT_CALL(*m_mockBlockDevOpenFile, pwrite(_, _, _)).Times(0);
    }

    dev_t deviceNum() const { return m_devNo; }

private:
    const dev_t m_devNo;
    std::shared_ptr<MockFile> m_mockBlockDevFile;
    std::shared_ptr<MockOpenFile> m_mockBlockDevOpenFile;
};

// -------------------------------------------------------------------------
/*!
    Wrapper around a mock file representing the device mapper control device.
    This is used to intercept the ioctl calls and map to indivdual mock methods
    for each ioctl command - for easier mocking in tests.

 */
class MockDMControlDevice
{
public:
    MockDMControlDevice()
    {
        char controlPath[64];
        snprintf(controlPath, sizeof(controlPath), "/dev/%s/%s", DM_DIR, DM_CONTROL_NODE);
        m_mockCtrlDevice = MockFile::registerPath(controlPath);
        m_mockOpenCtrlDevice = std::make_shared<MockOpenFile>();

        EXPECT_CALL(*m_mockCtrlDevice, open(StrEq(controlPath), O_CLOEXEC | O_RDWR, _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(m_mockOpenCtrlDevice));

        // Route the ioctl calls to the individual mock methods
        EXPECT_CALL(*m_mockOpenCtrlDevice, ioctl(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(
                [this](unsigned long request, void *arg) -> int
                {
                    auto *cmd = static_cast<dm_ioctl *>(arg);
                    switch (request)
                    {
                        case DM_VERSION:
                            return ioctl_DM_VERSION(cmd);
                        case DM_LIST_DEVICES:
                            return ioctl_DM_LIST_DEVICES(cmd);
                        case DM_DEV_CREATE:
                            return ioctl_DM_DEV_CREATE(cmd);
                        case DM_TABLE_LOAD:
                            return ioctl_DM_TABLE_LOAD(cmd);
                        case DM_DEV_SUSPEND:
                            return ioctl_DM_DEV_SUSPEND(cmd);
                        case DM_DEV_REMOVE:
                            return ioctl_DM_DEV_REMOVE(cmd);
                        case DM_DEV_STATUS:
                            return ioctl_DM_DEV_STATUS(cmd);
                        case DM_TABLE_STATUS:
                            return ioctl_DM_TABLE_STATUS(cmd);

                        default:
                            EXPECT_TRUE(false) << "Unexpected ioctl request: " << request;
                            return -1;
                    }
                });

        // Don't support any of these operations on the block device
        EXPECT_CALL(*m_mockOpenCtrlDevice, fstat(_)).Times(0);
        EXPECT_CALL(*m_mockOpenCtrlDevice, read(_, _)).Times(0);
        EXPECT_CALL(*m_mockOpenCtrlDevice, pread(_, _, _)).Times(0);
        EXPECT_CALL(*m_mockOpenCtrlDevice, write(_, _)).Times(0);
        EXPECT_CALL(*m_mockOpenCtrlDevice, pwrite(_, _, _)).Times(0);

        // Expect a single close call
        EXPECT_CALL(*m_mockOpenCtrlDevice, close()).Times(1).WillOnce(Return(0));
    }

    MOCK_METHOD(int, ioctl_DM_VERSION, (dm_ioctl * arg));
    MOCK_METHOD(int, ioctl_DM_LIST_DEVICES, (dm_ioctl * arg));
    MOCK_METHOD(int, ioctl_DM_DEV_CREATE, (dm_ioctl * arg));
    MOCK_METHOD(int, ioctl_DM_TABLE_LOAD, (dm_ioctl * arg));
    MOCK_METHOD(int, ioctl_DM_DEV_SUSPEND, (dm_ioctl * arg));
    MOCK_METHOD(int, ioctl_DM_DEV_REMOVE, (dm_ioctl * arg));
    MOCK_METHOD(int, ioctl_DM_DEV_STATUS, (dm_ioctl * arg));
    MOCK_METHOD(int, ioctl_DM_TABLE_STATUS, (dm_ioctl * arg));

private:
    std::shared_ptr<MockFile> m_mockCtrlDevice;
    std::shared_ptr<MockOpenFile> m_mockOpenCtrlDevice;
};

// -------------------------------------------------------------------------
/*!
    Helper to fill a buffer with dm_name_list entries for the given list of
    devices. Returns the total size used in the buffer.

*/
static size_t populateDmNameList(void *buffer, const std::list<DevMapper::MappedDevice> &devices)
{
    // populate the buffer with the dm_name_list entries as the kernel would
    size_t offset = 0;
    dm_name_list *entry = nullptr;
    for (const auto &dev : devices)
    {
        entry = reinterpret_cast<dm_name_list *>(reinterpret_cast<uint8_t *>(buffer) + offset);
        entry->dev = dev.deviceNumber;
        entry->next = sizeof(dm_name_list) + dev.name.size() + 1;
        strcpy(entry->name, dev.name.c_str());
        offset += entry->next;
    }

    // looking at the kernel code, the last entry's 'next' should be 0
    if (entry)
        entry->next = 0;

    return offset;
}

namespace entos::ralf::dmverity
{
    bool operator==(const DevMapper::MappedDevice &lhs, const DevMapper::MappedDevice &rhs)
    {
        return (lhs.deviceNumber == rhs.deviceNumber) && (lhs.name == rhs.name);
    }
} // namespace entos::ralf::dmverity

TEST_F(DevMapperTests, testGetMappedDevices)
{
    MockDMControlDevice mockDmControlDevice;

    EXPECT_CALL(mockDmControlDevice, ioctl_DM_VERSION(_))
        .WillRepeatedly(
            [&](dm_ioctl *cmd) -> int
            {
                cmd->version[0] = 4;
                cmd->version[1] = 7;
                cmd->version[2] = 8;
                return 0;
            });

    // Now create the DevMapper instance and try the APIs
    auto devMapper = std::make_unique<DevMapper>();
    ASSERT_TRUE(devMapper->isAvailable());
    ASSERT_EQ(devMapper->version(), entos::ralf::VersionNumber(4, 7, 8));

    // Test driver returning an error
    {
        EXPECT_CALL(mockDmControlDevice, ioctl_DM_LIST_DEVICES(_)).WillOnce(Return(-1));

        auto result = devMapper->mappedDevices();
        EXPECT_TRUE(result.isError()) << "Unexpected success getting mapped devices";
    }

    // Test driver returning no devices
    {
        EXPECT_CALL(mockDmControlDevice, ioctl_DM_LIST_DEVICES(_))
            .WillOnce(
                [&](dm_ioctl *cmd) -> int
                {
                    cmd->data_size = sizeof(dm_ioctl);
                    cmd->data_start = sizeof(dm_ioctl);
                    return 0;
                });

        auto result = devMapper->mappedDevices();
        EXPECT_TRUE(result.hasValue());
        EXPECT_TRUE(result.value().empty());
    }

    // Test driver returning no devices
    {
        const std::list<DevMapper::MappedDevice> expectedDevices = {
            { makedev(12, 34), "terrys" },
            { makedev(256, 123), "chocolate" },
            { makedev(45, 56), "orange" },
        };

        EXPECT_CALL(mockDmControlDevice, ioctl_DM_LIST_DEVICES(_))
            .WillOnce(
                [&](dm_ioctl *cmd) -> int
                {
                    cmd->data_start = sizeof(dm_ioctl);
                    cmd->data_size =
                        cmd->data_start +
                        populateDmNameList(reinterpret_cast<uint8_t *>(cmd) + cmd->data_start, expectedDevices);
                    return 0;
                });

        auto result = devMapper->mappedDevices();
        ASSERT_TRUE(result.hasValue());
        EXPECT_EQ(result.value(), expectedDevices);
    }

    devMapper.reset();
}

TEST_F(DevMapperTests, testMapWithVerity)
{
    // Testing mapping a dm-verity device on a loopback file requires a lot of mocking to pretend to have an
    // actual loop device with a dm-verity image on it, plus a real dm-verity kernel module present.
    // Hence, there is a lot of setup here to mock out the various file operations.

    // Create a mock loopback device that is backed by a real file containing a dm-verity image
    const std::filesystem::path baseDir = TEST_DATA_DIR "/1mb";

    MockLoopbackDevice mockLoopDevice(baseDir / "concatenated.img");
    const int loopDevFd = mockLoopDevice.fd();
    EXPECT_GE(loopDevFd, 0);

    const auto rootHashStr = fileStrContents(baseDir / "roothash.txt");
    const auto rootHash = fromHex(rootHashStr);
    ASSERT_EQ(rootHash.size(), 32u);

    const std::string saltStr = fileStrContents(baseDir / "salt.txt");

    const size_t dataSize = 1024 * 1024;

    std::unique_ptr<MockBlockDevice> mockBlockDevice;

    // Create a mock device mapper control device
    MockDMControlDevice mockDmControlDevice;
    EXPECT_CALL(mockDmControlDevice, ioctl_DM_VERSION(_))
        .WillOnce(
            [&](dm_ioctl *cmd) -> int
            {
                cmd->version[0] = 4;
                cmd->version[1] = 6;
                cmd->version[2] = 0;
                return 0;
            });

    // Expect a call to list devices
    EXPECT_CALL(mockDmControlDevice, ioctl_DM_LIST_DEVICES(_))
        .WillOnce(
            [&](dm_ioctl *cmd) -> int
            {
                cmd->data_size = sizeof(dm_ioctl);
                cmd->data_start = sizeof(dm_ioctl);
                cmd->data_size =
                    cmd->data_start +
                    populateDmNameList(reinterpret_cast<uint8_t *>(cmd) + cmd->data_start,
                                       { { makedev(12, 34), "terrys-chocolate-origin" },
                                         { makedev(256, 123), "qwerty-sinbad-roger" },
                                         { makedev(45, 56), "some-really-really-really-really-really-long-name" } });
                return 0;
            });

    // Expect a call to create the device
    EXPECT_CALL(mockDmControlDevice, ioctl_DM_DEV_CREATE(_))
        .WillOnce(
            [&](dm_ioctl *cmd) -> int
            {
                EXPECT_EQ(cmd->version[0], 4);
                EXPECT_EQ(cmd->version[1], 0);
                EXPECT_EQ(cmd->version[2], 0);

                EXPECT_STREQ(cmd->name, "foo");
                EXPECT_STREQ(cmd->uuid, "bar");
                return 0;
            });

    // Expect a call to load the verity table
    EXPECT_CALL(mockDmControlDevice, ioctl_DM_TABLE_LOAD(_))
        .WillOnce(
            [&](dm_ioctl *cmd) -> int
            {
                EXPECT_STREQ(cmd->name, "foo");
                EXPECT_STREQ(cmd->uuid, "");
                EXPECT_EQ(cmd->data_start, sizeof(dm_ioctl));
                EXPECT_EQ(cmd->target_count, 1);
                EXPECT_EQ(cmd->flags, DM_EXISTS_FLAG | DM_SECURE_DATA_FLAG | DM_READONLY_FLAG);

                auto *target = reinterpret_cast<dm_target_spec *>(reinterpret_cast<uint8_t *>(cmd) + sizeof(dm_ioctl));
                EXPECT_EQ(target->sector_start, 0);
                EXPECT_EQ(target->length, dataSize / 512);
                EXPECT_STREQ(target->target_type, "verity");

                // see https://docs.kernel.org/admin-guide/device-mapper/verity.html for dm-verity target params format
                char expected[512];
                snprintf(expected, sizeof(expected), "1 %u:%u %u:%u %u %u %zu %zu sha256 %s %s",
                         mockLoopDevice.majorNum(), mockLoopDevice.minorNum(), // data device
                         mockLoopDevice.majorNum(), mockLoopDevice.minorNum(), // hashes device
                         DATA_BLOCK_SIZE,                                      // data block size
                         HASH_BLOCK_SIZE,                                      // hash block size
                         (dataSize / DATA_BLOCK_SIZE),                         // number of data blocks
                         (dataSize / HASH_BLOCK_SIZE) + 1, // offset to first hash block (after superblock)
                         rootHashStr.c_str(),              // root hash
                         saltStr.c_str());                 // salt

                auto *params = reinterpret_cast<char *>(reinterpret_cast<uint8_t *>(cmd) + sizeof(dm_ioctl) +
                                                        sizeof(dm_target_spec));
                EXPECT_STREQ(params, expected);

                EXPECT_GE(cmd->data_size, sizeof(dm_ioctl) + sizeof(dm_target_spec) + strlen(params));

                return 0;
            });

    // Expect a call to activate the device (yes, weirdly this is done via the SUSPEND ioctl)
    EXPECT_CALL(mockDmControlDevice, ioctl_DM_DEV_SUSPEND(_))
        .WillOnce(
            [&](dm_ioctl *cmd) -> int
            {
                EXPECT_STREQ(cmd->name, "foo");
                EXPECT_STREQ(cmd->uuid, "");
                EXPECT_EQ(cmd->flags, DM_EXISTS_FLAG | DM_SECURE_DATA_FLAG | DM_READONLY_FLAG);
                EXPECT_EQ(cmd->event_nr,
                          ((DM_UDEV_DISABLE_LIBRARY_FALLBACK | DM_UDEV_PRIMARY_SOURCE_FLAG) << DM_UDEV_FLAGS_SHIFT));

                mockBlockDevice = std::make_unique<MockBlockDevice>(253, 123);
                cmd->dev = mockBlockDevice->deviceNum();

                return 0;
            });

    // Now create the DevMapper instance and try the APIs
    auto devMapper = std::make_unique<DevMapper>();
    ASSERT_TRUE(devMapper->isAvailable());
    ASSERT_EQ(devMapper->version(), entos::ralf::VersionNumber(4, 6, 0));

    auto mappedDevices = devMapper->mappedDevices();
    EXPECT_FALSE(mappedDevices.isError()) << "Failed to get a list of devices: " << mappedDevices.error().what();

    auto result = devMapper->mapWithVerity(loopDevFd, "foo", "bar", dataSize, dataSize, rootHash, false);
    EXPECT_FALSE(result.isError()) << "Failed to map mock dm-verity device: " << result.error().what();

    mockBlockDevice.reset();
    devMapper.reset();
}

TEST_F(DevMapperTests, testUnmapDevice)
{
    MockDMControlDevice mockDmControlDevice;

    EXPECT_CALL(mockDmControlDevice, ioctl_DM_VERSION(_))
        .WillRepeatedly(
            [&](dm_ioctl *cmd) -> int
            {
                cmd->version[0] = 4;
                cmd->version[1] = 7;
                cmd->version[2] = 8;
                return 0;
            });

    // Now create the DevMapper instance and try the APIs
    auto devMapper = std::make_unique<DevMapper>();
    ASSERT_TRUE(devMapper->isAvailable());
    ASSERT_EQ(devMapper->version(), entos::ralf::VersionNumber(4, 7, 8));

    // Test unmap device
    {
        EXPECT_CALL(mockDmControlDevice, ioctl_DM_DEV_REMOVE(_))
            .WillOnce(
                [&](dm_ioctl *cmd) -> int
                {
                    EXPECT_STREQ(cmd->name, "foo");
                    EXPECT_STREQ(cmd->uuid, "");
                    EXPECT_EQ(cmd->flags, DM_EXISTS_FLAG);
                    EXPECT_EQ(cmd->event_nr,
                              ((DM_UDEV_DISABLE_LIBRARY_FALLBACK | DM_UDEV_PRIMARY_SOURCE_FLAG) << DM_UDEV_FLAGS_SHIFT));
                    return 0;
                });

        auto result = devMapper->unmap("foo", "bar");
        EXPECT_FALSE(result.isError()) << "Failed to unmap device: " << result.error().what();
    }

    // Test deferred unmap device
    {
        EXPECT_CALL(mockDmControlDevice, ioctl_DM_DEV_REMOVE(_))
            .WillOnce(
                [&](dm_ioctl *cmd) -> int
                {
                    EXPECT_STREQ(cmd->name, "foo");
                    EXPECT_STREQ(cmd->uuid, "");
                    EXPECT_EQ(cmd->flags, (DM_EXISTS_FLAG | DM_DEFERRED_REMOVE));
                    EXPECT_EQ(cmd->event_nr,
                              ((DM_UDEV_DISABLE_LIBRARY_FALLBACK | DM_UDEV_PRIMARY_SOURCE_FLAG) << DM_UDEV_FLAGS_SHIFT));
                    return 0;
                });

        auto result = devMapper->unmap("foo", "bar", true);
        EXPECT_FALSE(result.isError()) << "Failed to unmap device: " << result.error().what();
    }

    devMapper.reset();
}

TEST_F(DevMapperTests, testMapStatus)
{
    MockDMControlDevice mockDmControlDevice;

    EXPECT_CALL(mockDmControlDevice, ioctl_DM_VERSION(_))
        .WillRepeatedly(
            [&](dm_ioctl *cmd) -> int
            {
                cmd->version[0] = 5;
                cmd->version[1] = 6;
                cmd->version[2] = 7;
                return 0;
            });

    auto devMapper = std::make_unique<DevMapper>();
    ASSERT_TRUE(devMapper->isAvailable());
    ASSERT_EQ(devMapper->version(), entos::ralf::VersionNumber(5, 6, 7));

    // Test single map status
    {
        EXPECT_CALL(mockDmControlDevice, ioctl_DM_TABLE_STATUS(_))
            .WillOnce(
                [&](dm_ioctl *cmd) -> int
                {
                    EXPECT_STREQ(cmd->name, "foo");
                    EXPECT_STREQ(cmd->uuid, "");
                    EXPECT_EQ(cmd->data_start, sizeof(dm_ioctl));
                    EXPECT_GE(cmd->data_size, sizeof(dm_ioctl));
                    EXPECT_EQ(cmd->flags, (DM_EXISTS_FLAG | DM_NOFLUSH_FLAG));

                    auto targets = reinterpret_cast<dm_target_spec *>(reinterpret_cast<uint8_t *>(cmd) + cmd->data_start);
                    strcpy(targets->target_type, "verity");
                    targets->sector_start = 0;
                    targets->length = 2097152;
                    targets->status = 1;
                    targets->next = 0;

                    auto *params = reinterpret_cast<char *>(reinterpret_cast<uint8_t *>(cmd) + cmd->data_start +
                                                            sizeof(dm_target_spec));
                    strcpy(params, "some status");

                    cmd->target_count = 1;

                    return 0;
                });

        auto result = devMapper->mapStatus("foo", "bar");
        EXPECT_FALSE(result.isError()) << "Failed to get map status: " << result.error().what();
        EXPECT_STREQ(result.value().c_str(), "some status");
    }

    devMapper.reset();
}
