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

#include "core/IPackageEntryImpl.h"
#include "erofs/IErofsReader.h"

#include <filesystem>
#include <memory>

// -------------------------------------------------------------------------
/*!
    \class OCIErofsImageEntry

    Wrapper object that implements the IPackageEntryImpl interface for an
    EROFS entry.  This is used to bridge the IErofsEntry interface and the
    IPackageEntryImpl interface, so that the EROFS reader code can return
    EROFS entries as IPackageEntryImpl objects.

 */
class OCIErofsImageEntry final : public LIBRALF_NS::IPackageEntryImpl
{
public:
    explicit OCIErofsImageEntry(std::unique_ptr<entos::ralf::erofs::IErofsEntry> &&entry);

    ~OCIErofsImageEntry() final = default;

    const std::filesystem::path &path() const override;

    size_t size() const override;

    std::filesystem::perms permissions() const override;

    time_t modificationTime() const override;

    uid_t ownerId() const override;

    gid_t groupId() const override;

    std::filesystem::file_type type() const override;

    ssize_t read(void *buf, size_t size, LIBRALF_NS::Error *error) override;

    LIBRALF_NS::Result<size_t> writeTo(int directoryFd, size_t maxSize,
                                       LIBRALF_NS::Package::ExtractOptions options) override;

private:
    const std::unique_ptr<entos::ralf::erofs::IErofsEntry> m_entryRef;
    entos::ralf::erofs::IErofsEntry *const m_entry;
    size_t m_offset;
};
