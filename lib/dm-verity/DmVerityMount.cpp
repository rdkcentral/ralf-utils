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

#include "DmVerityMount.h"
#include "DevMapper.h"
#include "core/LogMacros.h"

#include <sys/mount.h>
#include <sys/stat.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::dmverity;

DmVerityMount::DmVerityMount(std::filesystem::path devicePath, std::filesystem::path mountPoint,
                             std::string_view volumeName, std::string_view volumeUuid)
    : m_devicePath(std::move(devicePath))
    , m_mountPoint(std::move(mountPoint))
    , m_volumeName(volumeName)
    , m_volumeUuid(volumeUuid)
{
    // Get the dev_t number of the device we've mounted, this is used to check if the device is still mounted by
    // running stat on the mount point.
    struct stat buf = {};
    if (stat(m_devicePath.c_str(), &buf) != 0)
    {
        logSysError(errno, "Failed to stat device node '%s'", m_devicePath.c_str());
        return;
    }
    if (!S_ISBLK(buf.st_mode))
    {
        logError("Mounted device '%s' is not a block device", m_devicePath.c_str());
        return;
    }

    m_deviceNumber = buf.st_rdev;
}

DmVerityMount::~DmVerityMount()
{
    if (!m_detached && !m_unmounted)
    {
        doUnmount();
    }
}

void DmVerityMount::doUnmount()
{
    ::umount2(m_mountPoint.c_str(), UMOUNT_NOFOLLOW);
}

bool DmVerityMount::isMounted() const
{
    if (m_unmounted)
        return false;

    if (!m_deviceNumber)
    {
        logError("Device number is not set, cannot check if mounted");
        return false;
    }

    // If the device number of the mount point matches the device number of the device, then we are still mounted.
    // If not then the device has been unmounted or the mount point is no longer valid.

    struct stat buf = {};
    if (stat(m_mountPoint.c_str(), &buf) != 0)
    {
        if (errno != ENOENT)
            logSysError(errno, "Failed to stat device node '%s'", m_devicePath.c_str());

        return false;
    }

    return (buf.st_dev == m_deviceNumber.value());
}

std::filesystem::path DmVerityMount::mountPoint() const
{
    if (m_unmounted)
        return {};

    return m_mountPoint;
}

void DmVerityMount::unmount()
{
    if (m_unmounted)
        return;

    doUnmount();
    m_unmounted = true;
}

void DmVerityMount::detach()
{
    m_detached = true;
}

std::string DmVerityMount::volumeName() const
{
    if (m_unmounted)
        return {};

    return m_volumeName;
}

std::string DmVerityMount::volumeUuid() const
{
    if (m_unmounted)
        return {};

    return m_volumeUuid;
}

MountStatus DmVerityMount::status() const
{
    if (!isMounted())
        return MountStatus::NotMounted;

    // Here we would typically check the status of the dm-verity device and return the appropriate status based on
    // whether there is any corruption detected
    DevMapper devMapper;
    if (!devMapper.isAvailable())
    {
        logError("Device mapper is not available, cannot check mount status");
        return MountStatus::Mounted;
    }

    auto status = devMapper.mapStatus(m_volumeName, m_volumeUuid);
    if (!status)
    {
        logError("Failed to get device mapper status: %s", status.error().what());
        return MountStatus::Mounted;
    }

    // For dm-verity the status line string is just a single character, a 'C' for corrupted and 'V' for valid
    // \see https://elixir.bootlin.com/linux/v5.4.277/source/drivers/md/dm-verity-target.c#L708
    if (status.value() == "C")
        return MountStatus::Corrupted;

    if (status.value() != "V")
        logError("Unexpected device mapper status '%s' for '%s'", status.value().c_str(), m_volumeName.c_str());

    return MountStatus::Mounted;
}
