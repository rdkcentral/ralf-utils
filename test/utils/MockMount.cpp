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

#include "MockMount.h"

std::shared_ptr<testing::NiceMock<MockMount>> MockMount::get()
{
    static std::shared_ptr<testing::NiceMock<MockMount>> instance;

    if (!instance)
        instance = std::make_shared<testing::NiceMock<MockMount>>();

    return instance;
}

// NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data)
{
    return MockMount::get()->mount(source, target, filesystemtype, mountflags, reinterpret_cast<const char *>(data));
}

int umount(const char *target)
{
    return MockMount::get()->umount(target);
}

int umount2(const char *target, int flags)
{
    return MockMount::get()->umount2(target, flags);
}

// NOLINTEND(readability-inconsistent-declaration-parameter-name)
