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

#include "FakeTime.h"

#include <cassert>
#include <ctime>
#include <queue>

#include <sys/time.h>

#if defined(__linux__)
#    include <sys/syscall.h>
#    include <unistd.h>
#endif

static std::queue<std::time_t> g_timesQueue;

FakeTime::FakeTime(time_t time)
{
    g_timesQueue.push(time);
}

FakeTime::FakeTime(std::chrono::system_clock::time_point time)
{
    g_timesQueue.push(std::chrono::system_clock::to_time_t(time));
}

FakeTime::FakeTime(int year, int month, int day, int hour, int minute, int second)
{
    std::tm info = {};
    info.tm_year = year - 1900;
    info.tm_mon = month;
    info.tm_mday = day - 1;
    info.tm_hour = hour;
    info.tm_min = minute;
    info.tm_sec = second;

    g_timesQueue.push(mktime(&info));
}

FakeTime::FakeTime(const std::string &str, const std::string &format)
{
    std::tm info = {};
    assert(strptime(str.c_str(), format.c_str(), &info) != nullptr);

    g_timesQueue.push(mktime(&info));
}

FakeTime::~FakeTime()
{
    g_timesQueue.pop();
}

#if defined(__APPLE__)

// -------------------------------------------------------------------------
/*!
    On OSX it was a bit tricky to interpose a function in an app itself.
    After a bit of experimentation found that the most reliable way was
    to get a library and use the DYLD_INTERPOSE macro to replace the time
    functions.

    \see https://medium.com/geekculture/code-injection-with-dyld-interposing-3008441c62dd
 */

#    define DYLD_INTERPOSE(_replacement, _replacee)                                                                    \
        __attribute__((used)) static struct                                                                            \
        {                                                                                                              \
            const void *replacement;                                                                                   \
            const void *replacee;                                                                                      \
        } _interpose_##_replacee                                                                                       \
            __attribute__((section("__DATA,__interpose"))) = { (const void *)(unsigned long)&_replacement,             \
                                                               (const void *)(unsigned long)&_replacee };

extern "C" int FakeGetTimeOfDay(struct timeval *__restrict tv, void *__restrict tz)
{
    if (g_timesQueue.empty())
        return gettimeofday(tv, tz);

    tv->tv_sec = g_timesQueue.front();
    tv->tv_usec = 0;
    return 0;
}
DYLD_INTERPOSE(FakeGetTimeOfDay, gettimeofday);

extern "C" time_t FakeTime(time_t *tloc)
{
    if (g_timesQueue.empty())
        return time(tloc);

    if (tloc)
        *tloc = g_timesQueue.front();

    return g_timesQueue.front();
}
DYLD_INTERPOSE(FakeTime, time);

extern "C" int FakeClockGetTime(clockid_t clockid, struct timespec *tp)
{
    if (g_timesQueue.empty() || (clockid != CLOCK_REALTIME))
        return clock_gettime(clockid, tp);

    if (!tp)
        return -1;

    tp->tv_sec = g_timesQueue.front();
    tp->tv_nsec = 0;

    return 0;
}
DYLD_INTERPOSE(FakeClockGetTime, clock_gettime);

#elif defined(__linux__)

// -------------------------------------------------------------------------
/*!
    On Linux we can just interpose the time functions by statically linking
    the library with the app, as the symbols from the app take precedence
    over any dynamically loaded libraries.

    The linux version also uses syscalls to avoid any symbol resolution
    issues.

 */

// NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)

extern "C" int gettimeofday(struct timeval *tv, void *tz)
{
    if (g_timesQueue.empty())
        return static_cast<int>(syscall(SYS_gettimeofday, tv, tz));

    tv->tv_sec = g_timesQueue.front();
    tv->tv_usec = 0;
    return 0;
}

extern "C" time_t time(time_t *tloc)
{
    struct timeval tv = {};
    struct timezone tz = {};

    if (g_timesQueue.empty())
        syscall(SYS_gettimeofday, &tv, &tz);
    else
        tv.tv_sec = g_timesQueue.front();

    if (tloc)
        *tloc = tv.tv_sec;

    return tv.tv_sec;
}

extern "C" int clock_gettime(clockid_t clockid, struct timespec *tp)
{
    if (g_timesQueue.empty() || (clockid != CLOCK_REALTIME))
        return static_cast<int>(syscall(SYS_clock_gettime, clockid, tp));

    tp->tv_sec = g_timesQueue.front();
    tp->tv_nsec = 0;
    return 0;
}

// NOLINTEND(readability-inconsistent-declaration-parameter-name)

#else
#    error "Unsupported platform"
#endif
