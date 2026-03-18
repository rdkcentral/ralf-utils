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

#include "IOCIBackingStore.h"

class OCIMappableFile final : public IOCIMappableFile
{
public:
    OCIMappableFile(int fd, uint64_t offset, uint64_t size);
    ~OCIMappableFile() final;

    OCIMappableFile(const OCIMappableFile &) = delete;
    OCIMappableFile &operator=(const OCIMappableFile &) = delete;
    OCIMappableFile(OCIMappableFile &&) = delete;
    OCIMappableFile &operator=(OCIMappableFile &&) = delete;

    int fd() const override { return m_fd; }

    uint64_t offset() const override { return m_offset; }

    uint64_t size() const override { return m_size; }

    bool isAligned() const override;

private:
    int m_fd = -1;
    uint64_t m_offset = 0;
    uint64_t m_size = 0;
};
