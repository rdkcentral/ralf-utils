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

#include "IErofsImageFile.h"
#include "Result.h"

#include <filesystem>
#include <optional>

namespace entos::ralf::erofs
{
    // -----------------------------------------------------------------------------
    /*!
        \class ErofsRawImageFile
        \brief Simple read wrapper around a file descriptor.

        This implements the IErofsImageFile interface, used by IErofsReader to
        read blocks from a file.  In normal use the IErofsReader would use a
        IDmVerityProtectedFile object so that every block read from the file is checked
        against dm-verity hash tree, however this is a helper for non-dmverity
        protected image files.

     */
    class ErofsRawImageFile : public IErofsImageFile
    {
    public:
        static LIBRALF_NS::Result<std::unique_ptr<ErofsRawImageFile>> open(const std::filesystem::path &filePath,
                                                                           std::optional<size_t> offset = std::nullopt,
                                                                           std::optional<size_t> size = std::nullopt);
        static LIBRALF_NS::Result<std::unique_ptr<ErofsRawImageFile>>
        open(int fd, std::optional<size_t> offset = std::nullopt, std::optional<size_t> size = std::nullopt);

        ~ErofsRawImageFile() override;

        size_t size() const override;

        bool read(void *buffer, size_t size, size_t offset) override;

        bool advise(Advice advice, size_t size, size_t offset) override;

    private:
        ErofsRawImageFile(int fd, size_t offset, size_t size);

    private:
        const int m_fd;
        const size_t m_offset;
        const size_t m_size;
    };

} // namespace entos::ralf::erofs