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

#include "Result.h"

#include <chrono>
#include <cstdint>

#include <sys/sem.h>

namespace entos::ralf::dmverity
{

    // -------------------------------------------------------------------------
    /*!
        \class UDevNotifySemaphore
        \brief Helper class to manage a System V semaphore used to notify udev.

        When creating device-mapper devices you need to notify udev so that it
        can create the appropriate device nodes. This is done by setting the
        event_nr field in the dm_ioctl structure to a value that encodes the
        notification type and a semaphore id. Udev will then perform a semop()
        on the semaphore to signal that it has processed the device.

        This class wraps the creation and management of the semaphore used for
        this notification.

        \warning To use this class your system must be running udev and have the
        appropriate udev rules to handle the DM_COOKIE environment variable.
        See https://gitlab.com/lvmteam/lvm2/-/blob/main/udev/95-dm-notify.rules.in?ref_type=heads

     */
    class UDevNotifySemaphore
    {
    public:
        static LIBRALF_NS::Result<UDevNotifySemaphore> create();

    public:
        ~UDevNotifySemaphore();

        UDevNotifySemaphore(const UDevNotifySemaphore &) = delete;
        UDevNotifySemaphore &operator=(const UDevNotifySemaphore &) = delete;

        UDevNotifySemaphore(UDevNotifySemaphore &&rhs) noexcept;
        UDevNotifySemaphore &operator=(UDevNotifySemaphore &&rhs) noexcept;

        int id() const;
        uint32_t cookie() const;

        template <class Rep, class Period>
        inline LIBRALF_NS::Result<> wait(const std::chrono::duration<Rep, Period> &timeout)
        {
            return doWait(std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
        }

    private:
        UDevNotifySemaphore(int semId, uint32_t cookie);

        LIBRALF_NS::Result<> doWait(const std::chrono::milliseconds &timeout) const;

    private:
        int m_semId;
        uint32_t m_cookie;
    };

} // namespace entos::ralf::dmverity