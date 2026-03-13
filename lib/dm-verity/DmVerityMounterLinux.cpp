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

#include "DmVerityMounterLinux.h"
#include "DevMapper.h"
#include "DmVerityMount.h"
#include "DmVeritySuperBlock.h"
#include "Error.h"
#include "core/Compatibility.h"
#include "core/LogMacros.h"

#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstring>
#include <map>
#include <set>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::dmverity;

// clang-format off
/// Macros for dealing with stuff on block boundaries
#define ROUND_UP(n, s) ((((n) + (s) - 1) / (s)) * (s))
#define DIV_ROUND_UP(n, d) (((n) + ((d) - 1)) / (d))
// clang-format on

/// The number of bytes in a sha256 hash block
#define SHA256_DIGEST_LENGTH (32)

static const std::map<IDmVerityMounter::FileSystemType, const char *> fsTypeNames = {
    { IDmVerityMounter::FileSystemType::Erofs, "erofs" },
    { IDmVerityMounter::FileSystemType::Squashfs, "squashfs" },
    { IDmVerityMounter::FileSystemType::Ext3, "ext3" },
    { IDmVerityMounter::FileSystemType::Ext4, "ext4" },
};

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Converts a byte array \a bytes or \a length to a lower case hex string.

 */
static std::string bytesToHexString(const uint8_t *bytes, size_t length)
{
    static const char hexChars[17] = "0123456789abcdef";

    std::string str;
    str.reserve(length * 2 + 1);

    for (size_t i = 0; i < length; i++)
    {
        str.push_back(hexChars[((bytes[i] >> 4) & 0xf)]);
        str.push_back(hexChars[((bytes[i] >> 0) & 0xf)]);
    }

    return str;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Creates a unique device name and UUID string for the dm-verity device. The
    name is prefixed with "ralf--" followed by the \a id, which is the
    package ID, and a random set of characters to ensure uniqueness. The UUID
    is similar but also includes the \a uuid bytes converted to a hex string.

    A UUID is not strictly required for dm-verity, but I've included it to match
    the behaviour of the cryptsetup utility which creates a UUID.

    The function checks the existing mapped devices in the DevMapper to ensure
    that the generated name and UUID are unique.  It is racy in that some other
    process could create a device with the same name or UUID after this function
    has returned, but that is unlikely to happen in practice.

 */
static Result<std::pair<std::string, std::string>> createDeviceNameAndUuid(const DevMapper &devMapper,
                                                                           std::string_view id, const uint8_t uuid[16])
{
    // Get a list of current devices to ensure we can create a unique name and uuid
    auto existingDevices = devMapper.mappedDevices();
    if (!existingDevices)
        return existingDevices.error();

    auto existingDeviceNames = std::set<std::string>();
    for (const auto &device : existingDevices.value())
    {
        logInfo("Found existing devmapper device %u:%u with name '%s'", major(device.deviceNumber),
                minor(device.deviceNumber), device.name.c_str());

        existingDeviceNames.insert(device.name);
    }

    static const char *letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    timeval tv = {};
    gettimeofday(&tv, nullptr);
    uint64_t value = ((uint64_t)tv.tv_usec << 16) ^ tv.tv_sec;

    char volumeName[128];
    char volumeUuid[128];

    const std::string uuidStr = bytesToHexString(uuid, 16);

    const char *idStr = id.data();
    int idLength = static_cast<int>(std::min<size_t>(id.length(), 64));

    for (int attempts = 0; attempts < 100; attempts++, value += 7777)
    {
        char randomChars[7];
        randomChars[0] = letters[value % 62];
        value /= 62;
        randomChars[1] = letters[value % 62];
        value /= 62;
        randomChars[2] = letters[value % 62];
        value /= 62;
        randomChars[3] = letters[value % 62];
        value /= 62;
        randomChars[4] = letters[value % 62];
        value /= 62;
        randomChars[5] = letters[value % 62];
        randomChars[6] = '\0';

        snprintf(volumeName, sizeof(volumeName), "ralf--%.*s--%s", idLength, idStr, randomChars);
        snprintf(volumeUuid, sizeof(volumeUuid), "ralf-%s-%.*s-%s", uuidStr.c_str(), idLength, idStr, randomChars);

        if (existingDeviceNames.count(volumeName) == 0)
            return std::make_pair<std::string, std::string>(volumeName, volumeUuid);
    }

    return Error(ErrorCode::DmVerityError, "Failed to create unique device name and/or uuid after 100 attempts");
}

// -----------------------------------------------------------------------------
/**
    \internal
    \static

    Simply created the path `/proc/self/fd/<fd>`.

 */
static std::filesystem::path procPathToFd(int fd)
{
    return std::filesystem::path("/proc/self/fd") / std::to_string(fd);
}

// -----------------------------------------------------------------------------
/**
    \internal
    \static

    Given a number of \a dataBlocks of size \a blockSize the function calculates
    the number hash blocks required in the dm-verity merkle hash tree.

    Assumes a sha256 hash algo.

 */
static size_t calcRequiredHashBlocks(size_t dataBlocks, size_t blockSize)
{
    // Special case if a single data block, in which case the root hash stored in the header is just the hash of the
    // single data block, so no hash blocks
    if (dataBlocks == 1)
        return 0;

    const size_t hashesPerBlock = blockSize / SHA256_DIGEST_LENGTH;

    size_t layerHashBlocks = DIV_ROUND_UP(dataBlocks, hashesPerBlock);
    size_t requiredHashBlocks = layerHashBlocks;
    while (layerHashBlocks > 1)
    {
        layerHashBlocks = DIV_ROUND_UP(layerHashBlocks, hashesPerBlock);
        requiredHashBlocks += layerHashBlocks;
    }

    return requiredHashBlocks;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Finds a free loop device, returning a path to it's dev node.

 */
static Result<std::filesystem::path> getFreeLoopDevice()
{
    int loopControlFd = open("/dev/loop-control", O_RDONLY | O_CLOEXEC);
    if (loopControlFd < 0)
        return Error(std::error_code(errno, std::system_category()), "Failed to open loop-control dev node");

    int rc = ioctl(loopControlFd, LOOP_CTL_GET_FREE);
    if (rc < 0)
    {
        auto err = std::error_code(errno, std::system_category());

        close(loopControlFd);
        return Error(err, "Failed to get available free loop device");
    }

    close(loopControlFd);

    char pathBuf[64];
    snprintf(pathBuf, sizeof(pathBuf), "/dev/loop%d", rc);
    std::filesystem::path loopDevPath = pathBuf;

    struct stat buf = {};
    if ((stat(loopDevPath.c_str(), &buf) != 0) || !S_ISBLK(buf.st_mode))
    {
        return Error::format(std::error_code(errno, std::system_category()), "Failed to access loop device at '%s'",
                             loopDevPath.c_str());
    }

    return loopDevPath;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Attempts to attach the supplied image file fd to new loop block device. On
    success an fd to the new /dev/loopX device is returned, on failed \c -1 is
    returned.

 */
static Result<int> loopDeviceAttach(int imageFd, IDmVerityMounter::FileRange fileRange, MountFlags flags)
{
    // Loop device creation is racey with respect to other processes also creating loop devices, hence the retry loop
    int loopDevFd = -1;
    std::filesystem::path loopDevPath;
    while (loopDevFd < 0)
    {
        auto loopDevicePath = getFreeLoopDevice();
        if (!loopDevicePath)
        {
            return loopDevicePath.error();
        }

        int openFlags = O_CLOEXEC;
        if ((flags & MountFlag::ReadOnly) == MountFlag::ReadOnly)
            openFlags |= O_RDONLY;
        else
            openFlags |= O_RDWR;

        loopDevPath = loopDevicePath.value();
        loopDevFd = open(loopDevPath.c_str(), openFlags);
        if (loopDevFd < 0)
        {
            return Error::format(std::error_code(errno, std::system_category()), "Failed to open loop device at '%s'",
                                 loopDevPath.c_str());
        }

        if (ioctl(loopDevFd, LOOP_SET_FD, imageFd) < 0)
        {
            const int err = errno;
            close(loopDevFd);
            loopDevFd = -1;

            loopDevPath.clear();

            // if errno is EBUSY then it means someone got there before us and claimed the loop device, so go back
            // around again and try and find another free device
            if (err != EBUSY)
            {
                return Error::format(std::error_code(errno, std::system_category()),
                                     "LOOP_SET_FD ioctl failed on @ '%s'", loopDevPath.c_str());
            }
        }
    }

    loop_info64 info = {};
    memset(&info, 0x00, sizeof(info));

    // We always set autoclear, even if the caller didn't request it, because we always want the loop device to be
    // cleared when the devmapper device is removed.  The MountFlag::AutoClear is used to indicate at the devmapper
    // level to remove the mapping when the device is unmounted.
    info.lo_flags |= LO_FLAGS_AUTOCLEAR;

    // LO_FLAGS_READ_ONLY is not set because apparently this is controlled by open type of the loop device fd

    // Use realpath on the /proc/self/fd/X symlink for the name, the name is just for reference, it's not actually used
    // by the loop device driver
    const std::filesystem::path procPath = procPathToFd(imageFd);
    char *imagePath = realpath(procPath.c_str(), nullptr);
    if (imagePath)
    {
        strncpy(reinterpret_cast<char *>(info.lo_file_name), imagePath, LO_NAME_SIZE - 1);
        free(imagePath);
    }

    // Set the file range we want in the loop back mount
    info.lo_offset = fileRange.offset;
    info.lo_sizelimit = fileRange.size;

    // Apply the loop settings
    if (ioctl(loopDevFd, LOOP_SET_STATUS64, &info) < 0)
    {
        auto err = std::error_code(errno, std::system_category());

        // clear the fd since we've failed
        (void)ioctl(loopDevFd, LOOP_CLR_FD, 0);
        close(loopDevFd);

        return Error::format(err, "Failed to set the loopdevice flags on @ '%s'", loopDevPath.c_str());
    }

    // Set the directio flag
    if ((flags & MountFlag::DirectIO) == MountFlag::DirectIO)
    {
        unsigned long enable = 1;
        if (ioctl(loopDevFd, LOOP_SET_DIRECT_IO, enable) < 0)
        {
            auto err = std::error_code(errno, std::system_category());

            // clear the fd since we've failed
            (void)ioctl(loopDevFd, LOOP_CLR_FD, 0);
            close(loopDevFd);

            return Error::format(err, "Failed to set the loopdevice directio flag on @ '%s'", loopDevPath.c_str());
        }
    }

    return loopDevFd;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Reads and sanity checks the dmverity superblock from the given \a imageFileFd
    at the given \a offset.

 */
static Result<VeritySuperBlock> readDmVeritySuperBlock(int imageFileFd, IDmVerityMounter::FileRange dataRange,
                                                       IDmVerityMounter::FileRange hashesRange)
{
    constexpr uint64_t blockSize = 4096;

    if (hashesRange.size < blockSize)
        return Error(ErrorCode::DmVerityError, "dmverity hashes range is smaller than superblock size");

    // If O_DIRECT was used to open the image file then all reads need to block aligned, hence the allocation of
    // memaligned buffer
    std::unique_ptr<uint8_t, decltype(std::free) *> alignedBuf(reinterpret_cast<uint8_t *>(
                                                                   std::aligned_alloc(4096, blockSize)),
                                                               std::free);

    // Read the super block
    const ssize_t rd = TEMP_FAILURE_RETRY(pread(imageFileFd, alignedBuf.get(), blockSize, hashesRange.offset));
    if (rd != blockSize)
        return Error(std::error_code(errno, std::system_category()), "Failed to read image file");

    // Sanity check the superblock fields
    const auto *superBlock = reinterpret_cast<const VeritySuperBlock *>(alignedBuf.get());
    auto checkResult = checkSuperBlock(superBlock);
    if (!checkResult)
        return checkResult.error();

    // Check that the dmverity data blocks fit within the data file range
    if ((superBlock->dataBlocks * superBlock->dataBlockSize) > dataRange.size)
    {
        return Error::format(ErrorCode::DmVerityError,
                             "dmverity super block has invalid number of data blocks (blocks: %" PRIu64 ", "
                             "data size: %" PRIu64 ")",
                             superBlock->dataBlocks, dataRange.size);
    }

    // Calculate the size of the hash tree for the given number of data blocks
    const size_t requiredHashBlocks = calcRequiredHashBlocks(superBlock->dataBlocks, blockSize);
    if ((requiredHashBlocks * superBlock->hashBlockSize) > (hashesRange.size - blockSize))
    {
        return Error::format(ErrorCode::DmVerityError,
                             "Not enough dm-verity hashes in file to cover the entire data (required hash blocks: %zu, "
                             "actual hashes size %" PRIu64 ")",
                             requiredHashBlocks, hashesRange.size);
    }

    // Copy the superblock structure and free the aligned memory buffer
    Result<VeritySuperBlock> result = *superBlock;
    alignedBuf.reset();

    return result;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Checks the supplied file ranges are valid for the supplied image file.

 */
static Result<> checkFileRanges(int imageFd, IDmVerityMounter::FileRange dataRange,
                                IDmVerityMounter::FileRange hashesRange)
{
    (void)imageFd;

    // All file offsets and sizes must be 4k aligned
    if (((dataRange.offset | dataRange.size | hashesRange.offset | hashesRange.size) & 0xfff) != 0)
    {
        return Error(ErrorCode::DmVerityError, "Data and hashes offsets and sizes must be 4k aligned");
    }

    // Check that the data part of the file is before the hashes (this is a requirement for dm-verity / devmapper driver)
    if (hashesRange.offset < (dataRange.offset + dataRange.size))
    {
        return Error(ErrorCode::DmVerityError, "dm-verity hashes must come after the data");
    }

    return Ok();
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Performs the steps to mount the image:
        1. Checks the arguments and image is valid
        2. Loopback mount the image
        3. Create a devmapper device with the dm-verity details
        4. Mount the devmapper device to the mount point

 */
static Result<std::unique_ptr<IPackageMountImpl>> doMount(std::string_view name, IDmVerityMounter::FileSystemType fsType,
                                                          int imageFd, const std::filesystem::path &mountPoint,
                                                          IDmVerityMounter::FileRange dataRange,
                                                          IDmVerityMounter::FileRange hashesRange,
                                                          const std::vector<uint8_t> &rootHash,
                                                          const std::vector<uint8_t> &salt, MountFlags flags)
{
    // Currently the salt is not used, we read it from the dm-verity superblock if needed. Maybe in the future we will
    // use to verify the salt matches what was expected.
    (void)salt;

    // Sanity check the mount point is a directory and it exists
    std::error_code err;
    auto status = std::filesystem::symlink_status(mountPoint, err);
    if (status.type() != std::filesystem::file_type::directory)
        return Error::format(ErrorCode::InvalidArgument, "Mount point '%s' is not a directory", mountPoint.c_str());

    // Sanity check the supplied file offset
    auto checkResult = checkFileRanges(imageFd, dataRange, hashesRange);
    if (!checkResult)
        return checkResult.error();

    // Read and check the dm-verity super block from the image
    const auto sb = readDmVeritySuperBlock(imageFd, dataRange, hashesRange);
    if (!sb)
        return sb.error();

    // Check that the data size in the dm-verity header matches the expected data range supplied to the mount call
    if (dataRange.size != (sb->dataBlocks * sb->dataBlockSize))
    {
        return Error::format(ErrorCode::DmVerityError,
                             "The data size in the package doesn't match dm-verity superblock"
                             " (expect %" PRIu64 ", actual %" PRIu64 ")",
                             dataRange.size, (sb->dataBlocks * sb->dataBlockSize));
    }

    // Create a devmapper interface and make sure it's supported
    DevMapper devMapper;
    if (!devMapper.isAvailable())
        return Error(ErrorCode::DmVerityError, "Device mapper is not available on this system");

    // Generate a unique device name and uuid for the dm-verity device
    auto volumeNameAndUuid = createDeviceNameAndUuid(devMapper, name, sb->uuid);
    if (!volumeNameAndUuid)
        return volumeNameAndUuid.error();

    const auto volumeName = std::move(volumeNameAndUuid->first);
    const auto volumeUuid = std::move(volumeNameAndUuid->second);

    logDebug("Generated devmapper device name '%s' and uuid '%s'", volumeName.c_str(), volumeUuid.c_str());

    // Find the outer bounds of the range
    const IDmVerityMounter::FileRange overallRange = { dataRange.offset,
                                                       (hashesRange.offset + hashesRange.size) - dataRange.offset };

    // Attach the image to a loop device
    auto loopDevFd = loopDeviceAttach(imageFd, overallRange, flags);
    if (!loopDevFd)
        return loopDevFd.error();

    // Map the loop device using devmapper with dm-verity details
    auto mapperDevPath = devMapper.mapWithVerity(loopDevFd.value(), volumeName, volumeUuid,
                                                 (hashesRange.offset - overallRange.offset), dataRange.size, rootHash,
                                                 (flags & MountFlag::UDevSync) == MountFlag::UDevSync);

    // Close the loop device (if successfully mapped then loop device is maintained by kernel)
    if (close(loopDevFd.value()) != 0)
        logSysError(errno, "failed to close loop device");

    // Check if mapping succeeded
    if (!mapperDevPath)
        return mapperDevPath.error();

    // Create the mount flags, we always mount read-only as it's a dm-verity image, so no writes are allowed.
    unsigned long mountFlags = MS_RDONLY;
    if ((flags & MountFlag::NoDevice) == MountFlag::NoDevice)
        mountFlags |= MS_NODEV;
    if ((flags & MountFlag::NoSuid) == MountFlag::NoSuid)
        mountFlags |= MS_NOSUID;
    if ((flags & MountFlag::NoExec) == MountFlag::NoExec)
        mountFlags |= MS_NOEXEC;

    // We can now mount the devmapper device
    if (::mount(mapperDevPath->c_str(), mountPoint.c_str(), fsTypeNames.at(fsType), mountFlags, nullptr) != 0)
    {
        err = std::error_code(errno, std::system_category());

        devMapper.unmap(volumeName, volumeUuid, false);
        return Error::format(err, "Failed to mount dm-verity image '%s' at '%s'", mapperDevPath->c_str(),
                             mountPoint.c_str());
    }

    // Check the status of the device mapper device, this will return an error if the device is not found.
    // For dm-verity the status string is just a single character, a 'C' for corrupted and 'V' for valid
    // \see https://elixir.bootlin.com/linux/v5.4.277/source/drivers/md/dm-verity-target.c#L708
    auto dmStatus = devMapper.mapStatus(volumeName, volumeUuid);
    if (!dmStatus || (dmStatus.value() != "V"))
    {
        ::umount2(mountPoint.c_str(), MNT_FORCE);
        devMapper.unmap(volumeName, volumeUuid, false);

        if (!dmStatus)
            return dmStatus.error();
        else
            return Error(ErrorCode::DmVerityError, "Detected corruption in dm-verity image, cannot mount");
    }

    // Can now remove the mapping using 'deferred', meaning while it's mounted the mapping will remain, when unmounted
    // the kernel will clean up the mapping (and close the loop device which was also marked with autoclear)
    if ((flags & MountFlag::NoAutoClear) != MountFlag::NoAutoClear)
    {
        if (!devMapper.unmap(volumeName, volumeUuid, true))
            logError("Failed to unmap the image loop device - this may lead to future failures");
    }

    // Create the PackageMount object to return details of the mounted image
    return std::make_unique<DmVerityMount>(mapperDevPath.value(), mountPoint, volumeName, volumeUuid);
}

Result<std::unique_ptr<IPackageMountImpl>>
DmVerityMounterLinux::mount(std::string_view name, FileSystemType fsType, const std::filesystem::path &imagePath,
                            const std::filesystem::path &mountPoint, FileRange dataRange, FileRange hashesRange,
                            const std::vector<uint8_t> &rootHash, const std::vector<uint8_t> &salt, MountFlags flags) const
{
    // Open the image file, optionally with the O_DIRECT flag
    int openFlags = O_CLOEXEC | O_RDONLY;
    if ((flags & MountFlag::DirectIO) == MountFlag::DirectIO)
        openFlags |= O_DIRECT;

    int imageFd = open(imagePath.c_str(), openFlags);
    if (imageFd < 0)
    {
        // Not all filesystems support O_DIRECT (ie. tmpfs), so if we failed try opening without the O_DIRECT flag
        if ((openFlags & O_DIRECT) && (errno == EINVAL))
        {
            flags &= ~MountFlag::DirectIO;
            openFlags &= ~O_DIRECT;

            imageFd = open(imagePath.c_str(), openFlags);
            if (imageFd >= 0)
            {
                logWarning("Requested mount using directio mode, but image file is on a fs that doesn't support "
                           "O_DIRECT, disabling directio");
            }
        }

        if (imageFd < 0)
        {
            return Error::format(std::error_code(errno, std::system_category()), "Failed to open file @ '%s'",
                                 imagePath.c_str());
        }
    }

    auto result = doMount(name, fsType, imageFd, mountPoint, dataRange, hashesRange, rootHash, salt, flags);

    if (close(imageFd) != 0)
        logSysError(errno, "Failed to close image file");

    return result;
}

Result<std::unique_ptr<IPackageMountImpl>>
DmVerityMounterLinux::mount(std::string_view name, FileSystemType fsType, int imageFd,
                            const std::filesystem::path &mountPoint, FileRange dataRange, FileRange hashesRange,
                            const std::vector<uint8_t> &rootHash, const std::vector<uint8_t> &salt, MountFlags flags) const
{
    // Check that if directio was requested that the supplied fd was opened with O_DIRECT, or try and set it now
    if ((flags & MountFlag::DirectIO) == MountFlag::DirectIO)
    {
        int ret = fcntl(imageFd, F_GETFL, 0);
        if (ret < 0)
        {
            return Error(std::error_code(errno, std::system_category()), "Failed to get flags on image fd");
        }

        if ((ret & O_DIRECT) == 0)
        {
            // directio is not set on the fd, so try and open a new version of the file with that flag set (you can use
            // F_SETFL to set the O_DIRECT flag on the fd, but this would be messing with the caller supplied fd)
            const std::filesystem::path procLink = procPathToFd(imageFd);
            return mount(name, fsType, procLink, mountPoint, dataRange, hashesRange, rootHash, salt, flags);
        }
    }

    return doMount(name, fsType, imageFd, mountPoint, dataRange, hashesRange, rootHash, salt, flags);
}

Result<> DmVerityMounterLinux::checkImageFile(FileSystemType fsType, int imageFd, FileRange dataRange,
                                              FileRange hashesRange) const
{
    (void)fsType;

    // Check the supplied file offsets
    auto checkResult = checkFileRanges(imageFd, dataRange, hashesRange);
    if (!checkResult)
        return checkResult.error();

    // Read the dmverity super block from the image
    const auto sb = readDmVeritySuperBlock(imageFd, dataRange, hashesRange);
    if (!sb)
        return sb.error();

    // Check that the data size in the dm-verity header matches the expected data range supplied to the mount call
    if (dataRange.size != (sb->dataBlocks * sb->dataBlockSize))
    {
        return Error::format(ErrorCode::DmVerityError,
                             "The data size in the package doesn't match dm-verity superblock"
                             " (expect %" PRIu64 ", actual %" PRIu64 ")",
                             dataRange.size, (sb->dataBlocks * sb->dataBlockSize));
    }

    return Ok();
}

Result<> DmVerityMounterLinux::checkImageFile(FileSystemType fsType, const std::filesystem::path &imagePath,
                                              FileRange dataRange, FileRange hashesRange) const
{
    // open the image file
    int imageFd = open(imagePath.c_str(), O_CLOEXEC | O_RDONLY);
    if (imageFd < 0)
    {
        return Error::format(std::error_code(errno, std::system_category()), "Failed to open image file @ '%s'",
                             imagePath.c_str());
    }

    auto result = checkImageFile(fsType, imageFd, dataRange, hashesRange);

    if (close(imageFd) != 0)
        logSysError(errno, "Failed to close image file");

    return result;
}
