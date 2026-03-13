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

#include "core/IPackageMountImpl.h"

#include <filesystem>
#include <optional>
#include <string>

namespace entos::ralf::dmverity
{

    // -------------------------------------------------------------------------
    /*!
    \class DmVerityMount
    \brief Implementation of IPackageMountImpl that represents a dm-verity
    mounted package.

    Most of the functionality is common to any object that represents a mount,
    however the status is dm-verity / devmapper specific as it reports if
    there is any corruption detected in the mounted package, which has
    security implications.

 */
    class DmVerityMount : public LIBRALF_NS::IPackageMountImpl
    {
    public:
        DmVerityMount(std::filesystem::path devicePath, std::filesystem::path mountPoint, std::string_view volumeName,
                      std::string_view volumeUuid);
        ~DmVerityMount() override;

        bool isMounted() const override;

        std::filesystem::path mountPoint() const override;

        void unmount() override;

        void detach() override;

        std::string volumeName() const override;

        std::string volumeUuid() const override;

        LIBRALF_NS::MountStatus status() const override;

    private:
        void doUnmount();

    private:
        const std::filesystem::path m_devicePath;
        const std::filesystem::path m_mountPoint;
        const std::string m_volumeName;
        const std::string m_volumeUuid;

        bool m_detached = false;
        bool m_unmounted = false;

        std::optional<dev_t> m_deviceNumber;
    };

} // namespace entos::ralf::dmverity