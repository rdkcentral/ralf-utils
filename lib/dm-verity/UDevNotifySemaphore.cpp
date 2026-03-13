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

#include "UDevNotifySemaphore.h"
#include "core/LogMacros.h"

#include <random>

#include <fcntl.h>
#include <sys/sem.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::dmverity;

// These are the flags for the event_nr field and are used by udev to determine how to handle the device.
// See https://salsa.debian.org/lvm-team/lvm2/-/blob/main/device_mapper/all.h
#define DM_COOKIE_MAGIC 0x0D4D
#define DM_UDEV_FLAGS_SHIFT 16

// Definition of semun is missing in some libc implementations
// See https://man7.org/linux/man-pages/man2/semctl.2.html
union semun
{
    int val;                 // Value for SETVAL
    struct semid_ds *buf;    // Buffer for IPC_STAT, IPC_SET
    unsigned short *array;   // Array for GETALL, SETALL
    struct seminfo *infoBuf; // Buffer for IPC_INFO (Linux-specific)
};

Result<UDevNotifySemaphore> UDevNotifySemaphore::create()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 0xFFFF);

    key_t key;
    int semaphoreId;

    while (true)
    {
        // Generate a new random key
        key = static_cast<key_t>((DM_COOKIE_MAGIC << 16) | dist(gen));

        // Try and create the semaphore
        semaphoreId = semget(key, 1, IPC_CREAT | IPC_EXCL | 0600);
        if (semaphoreId < 0)
        {
            if (errno == EEXIST)
            {
                // Semaphore already exists, try again with a new key
                continue;
            }
            else
            {
                // Some other error occurred
                return Error(std::error_code(errno, std::system_category()),
                             "Failed to create udev notify semaphore for device mapper");
            }
        }

        break;
    }

    logDebug("Created udev notify semaphore with key 0x%08x and id %d", key, semaphoreId);

    semun args = {};
    args.val = 1;

    // Initialise the semaphore to 1, this is due to SystemV semaphores having a wait-for-zero mode.  udev will
    // perform a semop() that decrements the semaphore to 0 to signal that it has processed the device.
    if (semctl(semaphoreId, 0, SETVAL, args) < 0)
    {
        int savedErrno = errno;

        // Failed to set the initial value, clean up and try again
        if (semctl(semaphoreId, 0, IPC_RMID, 0) < 0)
            logSysError(errno, "Failed to delete udev notify semaphore with key 0x%08x after initialisation failure",
                        key);

        return Error(std::error_code(savedErrno, std::system_category()),
                     "Failed to initialise udev notify semaphore for device mapper");
    }

    int val = semctl(semaphoreId, 0, GETVAL);
    if (val < 0)
    {
        int savedErrno = errno;

        // Failed to get the initial value, clean up and try again
        if (semctl(semaphoreId, 0, IPC_RMID, 0) < 0)
            logSysError(errno, "Failed to delete udev notify semaphore with key 0x%08x after GETVAL failure", key);

        return Error(std::error_code(savedErrno, std::system_category()),
                     "Failed to get initial value of udev notify semaphore for device mapper");
    }

    logInfo("udev cookie semaphore 0x%08x (id %d) has value set to %d", key, semaphoreId, val);

    return UDevNotifySemaphore(semaphoreId, static_cast<uint32_t>(key));
}

UDevNotifySemaphore::UDevNotifySemaphore(int semId, uint32_t cookie)
    : m_semId(semId)
    , m_cookie(cookie)
{
}

UDevNotifySemaphore::UDevNotifySemaphore(UDevNotifySemaphore &&rhs) noexcept
    : m_semId(rhs.m_semId)
    , m_cookie(rhs.m_cookie)
{
    rhs.m_semId = -1;
}

UDevNotifySemaphore &UDevNotifySemaphore::operator=(UDevNotifySemaphore &&rhs) noexcept
{
    if (this != &rhs)
    {
        if (m_semId >= 0)
        {
            if (semctl(m_semId, 0, IPC_RMID, 0) < 0)
                logSysError(errno, "Failed to delete udev notify semaphore with key 0x%08x in move assignment", m_cookie);
        }

        m_semId = rhs.m_semId;
        m_cookie = rhs.m_cookie;

        rhs.m_semId = -1;
    }

    return *this;
}

UDevNotifySemaphore::~UDevNotifySemaphore()
{
    if (m_semId >= 0)
    {
        if (semctl(m_semId, 0, IPC_RMID, 0) < 0)
            logSysError(errno, "Failed to delete udev notify semaphore with key 0x%08x in destructor", m_cookie);
    }
}

int UDevNotifySemaphore::id() const
{
    return m_semId;
}

uint32_t UDevNotifySemaphore::cookie() const
{
    return m_cookie;
}

Result<> UDevNotifySemaphore::doWait(const std::chrono::milliseconds &timeout) const
{
    if (m_semId < 0)
    {
        return Error(std::make_error_code(std::errc::bad_file_descriptor), "Invalid udev notify semaphore id in wait");
    }

    // System V semaphores are weird; and they have a mode called wait-for-zero, which means we create the semaphore
    // with a value of 1, and then udev does a semop() that decrements it to 0 to signal that it has processed the
    // device. With traditional semaphores you would expect to wait by decrementing the semaphore value, but with
    // System V semaphores you wait for the value to become zero.

    sembuf sb = { 0, 0, 0 };

    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(timeout);
    const auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout - secs);

    timespec ts = {};
    ts.tv_sec = static_cast<time_t>(secs.count());
    ts.tv_nsec = static_cast<time_t>(nsecs.count());

    if (semtimedop(m_semId, &sb, 1, &ts) < 0)
    {
        if (errno == EAGAIN)
        {
            return Error(std::make_error_code(std::errc::timed_out), "Timeout waiting for udev notify semaphore");
        }
        else
        {
            return Error(std::error_code(errno, std::system_category()), "Failed to wait on udev notify semaphore");
        }
    }

    logDebug("udev notify semaphore 0x%08x signalled by udev", m_cookie);

    return Ok();
}
