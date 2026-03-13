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

#include <cstddef>

namespace entos::ralf::erofs
{

    // -----------------------------------------------------------------------------
    /*!
        \interface IErofsImageFile
        \brief Simple object wrapper for reading a file.

        Why is it needed ? Because EROFS images are typically protected by a
        dm-verity hash tree, so by using an interface for reading the EROFS image
        we can do dm-verity hash check on all the data read from the file.

     */
    class IErofsImageFile
    {
    public:
        virtual ~IErofsImageFile() = default;

        // -------------------------------------------------------------------------
        /*!
            Returns the total size of the image file.

         */
        virtual size_t size() const = 0;

        // -------------------------------------------------------------------------
        /*!
            Performs a read of the given number of bytes at the given \a offset
            from the file and copies it into buffer.

            Returns \c true only if \a size bytes were successfully read and
            copied into the buffer.  This API doesn't allow partial reads.

         */
        virtual bool read(void *buffer, size_t size, size_t offset) = 0;

        // -------------------------------------------------------------------------
        /*!
            \enum Advice

            File access advice, see posix_fadvise for the meaning of the values.

            \see https://man7.org/linux/man-pages/man2/posix_fadvise.2.html
         */
        enum class Advice
        {
            Normal = 0,
            Sequential,
            Random,
            WillNeed,
            DontNeed
        };

        // -------------------------------------------------------------------------
        /*!
            Used to set the file access intention, this should just be a straight
            wrapper around posix_fadvise.

            The IErofsReader implementation may use this to remove parts of the
            file from the page cache once they've been read, or to trigger the
            kernel's read ahead logic on blocks from a contiguous file range.

            On failure \c false is returned and errno is set to the error reason
            (note this is different from posix_fadvise).

         */
        virtual bool advise(Advice advice, size_t size, size_t offset) = 0;
    };

} // namespace entos::ralf::erofs