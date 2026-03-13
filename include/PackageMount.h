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

#include "EnumFlags.h"
#include "LibRalf.h"

#include <filesystem>
#include <memory>
#include <string>

namespace LIBRALF_NS
{

    class IPackageMountImpl;

    // -----------------------------------------------------------------------------
    /*!
        Possible mount flags for when mounting a package.

    */
    enum class MountFlag : uint32_t
    {
        None = 0,
        ReadOnly = (1 << 0),
        NoSuid = (1 << 1),
        NoDevice = (1 << 2),
        NoExec = (1 << 3),

        DirectIO = (1 << 16),    //!< Use direct I/O for the mount, this is best effort and not all images / filesystems
                                 //!< support it.
        NoAutoClear = (1 << 17), //!< Don't automatically clear the loop device and / or dm device when unmounted.  You
                                 //!< can use this flag if you want to keep the loop device or dm device around after
                                 //!< unmounting, for example if you want to remount the package later.

        UDevSync = (1 << 24), //!< Use udev synchronization when mounting the package.  To use this flag your system
                              //!< must have udev installed and running, and it must have a LVM2 udev rule that
                              //!< processes the DM_COOKIE environment variable.
                              //!< See https://gitlab.com/lvmteam/lvm2/-/blob/main/udev/95-dm-notify.rules.in
    };

    LIBRALF_ENUM_FLAGS(MountFlags, MountFlag)

    enum class MountStatus : uint32_t
    {
        NotMounted = 0, //!< The package is not currently mounted.  Either PackageMount::unmount was called or some
                        //!< external process unmounted the package.
        Mounted,        //!< The package is mounted successfully and no errors have been detected.
        Corrupted,      //!< The package is mounted but a corruption has been detected, this can happen if the package
                        //!< was modified while it was mounted or if there was a hardware error.
    };

    // -----------------------------------------------------------------------------
    /*!
        \class PackageMount
        \brief Wrapper for a mounted package.

        Stores data on the mount point, volume name and UUID of the mounted package.
        On destruction it will unmount the package unless the detach() method
        was called before the destructor.

     */
    class LIBRALF_EXPORT PackageMount
    {
    public:
        PackageMount(const PackageMount &) = delete;
        PackageMount(PackageMount &&other) noexcept;

        PackageMount &operator=(const PackageMount &) = delete;
        PackageMount &operator=(PackageMount &&other) noexcept;

        // -------------------------------------------------------------------------
        /*!
            Destructor that will unmount the package unless the detach() method
            was called before the destructor.

         */
        ~PackageMount();

        // -------------------------------------------------------------------------
        /*!
            Returns \c true if the package is currently mounted, \c false otherwise.

         */
        bool isMounted() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the mount point of the mounted package.

            This will return an empty path if the package is not mounted.
         */
        std::filesystem::path mountPoint() const;

        // -------------------------------------------------------------------------
        /*!
            Performs an explicit unmount of the package.

            This will invalidate the mount points and mount UUID.
         */
        void unmount();

        // -------------------------------------------------------------------------
        /*!
            Detaches this object from the mount, so that when the destructor is
            called the package is NOT unmounted.

            This does not unmount the package, or remove the mount point, it just
            changes the ownership of the mount so that this object is no longer
            responsible for unmounting the package when it is destroyed.

         */
        void detach();

        // -------------------------------------------------------------------------
        /*!
            Returns the volume name of the mounted package.

            The volume name is used to identify the mount in /dev/mapper and is
            typically generated from the package id and a random suffix.  The volume
            name for be unique across all mounted packages, mounting the same package
            twice will result in a different volume name for each mount.

            This is the same name that is used in the /dev/mapper/<volume_name>
            and can be used to identify the mount in the udev events.

            This will return an empty string if the package is not mounted.
         */
        std::string volumeName() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the volume UUID of the mounted package.

            This will return an empty string if the package is not mounted.
         */
        std::string volumeUuid() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the status of the mount.  This is useful for detecting any errors
            with a package mount, for example if a corruption has been detected with
            the package while it was mounted.

            This uses the mount point and volume UUID to determine the status. Mounts
            can be changed by external processes, so this status reflects the current
            state of the mount not the logic state stored in this object.

            It is valid to call this API if detached() has been called.
         */
        MountStatus status() const;

    private:
        friend class Package;
        explicit PackageMount(std::unique_ptr<IPackageMountImpl> &&impl);

    private:
        std::unique_ptr<IPackageMountImpl> m_impl;
    };

} // namespace LIBRALF_NS
