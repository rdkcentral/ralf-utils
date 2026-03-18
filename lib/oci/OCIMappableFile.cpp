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

#include "OCIMappableFile.h"
#include "core/LogMacros.h"

#include <unistd.h>

OCIMappableFile::OCIMappableFile(int fd, uint64_t offset, uint64_t size)
    : m_fd(fd)
    , m_offset(offset)
    , m_size(size)
{
}

OCIMappableFile::~OCIMappableFile()
{
    if ((m_fd >= 0) && (close(m_fd) != 0))
        logSysError(errno, "Failed to close mappable file fd");
}

bool OCIMappableFile::isAligned() const
{
    // For now avoiding use of getpagesize() as it can return different values on different platforms, making it
    // less useful if running this library on host systems.
    // const int alignment = getpagesize();
    const int alignment = 4096;

    return ((m_offset % alignment) == 0) && ((m_size % alignment) == 0);
}